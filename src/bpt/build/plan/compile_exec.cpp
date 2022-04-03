#include "./compile_exec.hpp"

#include <bpt/build/file_deps.hpp>
#include <bpt/error/errors.hpp>
#include <bpt/util/log.hpp>
#include <bpt/util/parallel.hpp>
#include <bpt/util/proc.hpp>
#include <bpt/util/result.hpp>
#include <bpt/util/signal.hpp>
#include <bpt/util/string.hpp>
#include <bpt/util/time.hpp>

#include <fansi/styled.hpp>
#include <neo/assert.hpp>
#include <range/v3/algorithm/count_if.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <thread>

using namespace bpt;
using namespace ranges;
using namespace fansi::literals;

namespace {

/// Simple aggregate that stores a counter for keeping track of compile progress
struct compile_counter {
    std::atomic_size_t n{1};
    const std::size_t  max;
    const std::size_t  max_digits;
};

struct compile_ticket {
    std::reference_wrapper<const compile_file_plan> plan;
    // If non-null, the information required to compile the file
    compile_command_info command;
    fs::path             object_file_path;
    bool                 needs_recompile;
    // Information about the previous time a file was compiled, if any
    std::optional<completed_compilation> prior_command;
    // Whether this compilation is for the purpose of header independence
    bool is_syntax_only = false;
};

/**
 * Actually performs a compilation and collects deps information from that compilation
 *
 * @param cf The compilation to execute
 * @param env The build environment
 * @param counter A thread-safe counter for display progress to the user
 */
std::optional<file_deps_info>
handle_compilation(const compile_ticket& compile, build_env_ref env, compile_counter& counter) {
    if (!compile.needs_recompile) {
        // We don't actually compile this file. Just issue any prior warning messages that were from
        // a prior compilation.
        neo_assert(invariant,
                   compile.prior_command.has_value(),
                   "Expected a prior compilation command for file",
                   compile.plan.get().source_path(),
                   quote_command(compile.command.command));
        auto& prior = *compile.prior_command;
        if (bpt::trim_view(prior.output).empty()) {
            // Nothing to show
            return {};
        }
        if (!compile.plan.get().rules().enable_warnings()) {
            // This file shouldn't show warnings. The compiler *may* have produced prior output, but
            // this block will be hit when the source file belongs to an external dependency. Rather
            // than continually spam the user with warnings that belong to dependencies, don't
            // repeatedly show them.
            bpt_log(trace,
                    "Cached compiler output suppressed for file with disabled warnings ({})",
                    compile.plan.get().source_path().string());
            return {};
        }
        bpt_log(
            warn,
            "While compiling file .bold.cyan[{}] [.bold.yellow[{}]] (.br.blue[cached compiler output]):\n{}"_styled,
            compile.plan.get().source_path().string(),
            prior.quoted_command,
            prior.output);
        return {};
    }

    // Create the parent directory
    fs::create_directories(compile.object_file_path.parent_path());

    // Generate a log message to display to the user
    auto source_path = compile.plan.get().source_path();

    std::string_view compile_event_msg = compile.is_syntax_only ? "Check" : "Compile";
    auto             msg
        = fmt::format("[{}] {}: .br.cyan[{}]"_styled,
                      compile.plan.get().qualifier(),
                      compile_event_msg,
                      fs::relative(source_path, compile.plan.get().source().basis_path).string());

    // Do it!
    bpt_log(info, msg);
    auto start_time = fs::file_time_type::clock::now();
    auto&& [dur_ms, proc_res]
        = timed<std::chrono::milliseconds>([&] { return run_proc(compile.command.command); });
    auto nth = counter.n.fetch_add(1);
    bpt_log(info,
            "{:60} - {:>7L}ms [{:{}}/{}]",
            msg,
            dur_ms.count(),
            nth,
            counter.max_digits,
            counter.max);

    const bool  compiled_okay   = proc_res.okay();
    const auto  compile_retc    = proc_res.retc;
    const auto  compile_signal  = proc_res.signal;
    std::string compiler_output = std::move(proc_res.output);

    // Build dependency information, if applicable to the toolchain
    std::optional<file_deps_info> ret_deps_info;

    if (!compiled_okay) {
        /**
         * Do nothing: We failed to compile, so updating deps would be wasteful, and possibly wrong
         */
    } else if (env.toolchain.deps_mode() == file_deps_mode::gnu) {
        // GNU-style deps using Makefile generation
        assert(compile.command.gnu_depfile_path.has_value());
        auto& df_path = *compile.command.gnu_depfile_path;
        if (!fs::is_regular_file(df_path)) {
            bpt_log(critical,
                    "The expected Makefile deps were not generated on disk. This is a bug! "
                    "(Expected file to exist: [{}])",
                    df_path.string());
        } else {
            bpt_log(trace, "Loading compilation dependencies from {}", df_path.string());
            auto dep_info = bpt::parse_mkfile_deps_file(df_path);
            neo_assert(invariant,
                       dep_info.output == compile.object_file_path,
                       "Generated mkfile deps output path does not match the object file path that "
                       " we gave it to compile into.",
                       dep_info.output.string(),
                       compile.object_file_path.string());
            dep_info.command.quoted_command = quote_command(compile.command.command);
            dep_info.command.output         = compiler_output;
            dep_info.command.duration       = dur_ms;
            ret_deps_info                   = std::move(dep_info);
        }
    } else if (env.toolchain.deps_mode() == file_deps_mode::msvc) {
        // Uglier deps generation by parsing the output from cl.exe
        bpt_log(trace, "Parsing compilation dependencies from MSVC output");
        /// TODO: Handle different #include Note: prefixes, since those are localized
        auto msvc_deps = parse_msvc_output_for_deps(compiler_output, "Note: including file:");
        // parse_msvc_output_for_deps will return the compile output without the /showIncludes notes
        compiler_output = std::move(msvc_deps.cleaned_output);
        // Only update deps if we actually parsed anything, other wise we can't be sure that we
        // successfully parsed anything, and we don't want to store garbage deps info and possibly
        // cause a miscompile
        if (!msvc_deps.deps_info.inputs.empty()) {
            // Add the main source file as an input, since it is not listed by /showIncludes
            msvc_deps.deps_info.inputs.push_back(compile.plan.get().source_path());
            msvc_deps.deps_info.output                 = compile.object_file_path;
            msvc_deps.deps_info.command.quoted_command = quote_command(compile.command.command);
            msvc_deps.deps_info.command.output         = compiler_output;
            msvc_deps.deps_info.command.duration       = dur_ms;
            ret_deps_info                              = std::move(msvc_deps.deps_info);
        }
    } else {
        /**
         * We have no deps-mode set, so we can't really figure out what to do.
         */
    }

    if (ret_deps_info) {
        ret_deps_info->command.toolchain_hash = env.toolchain.hash();
        ret_deps_info->compile_start_time     = start_time;
    }

    // MSVC prints the filename of the source file. Remove it from the output.
    if (compiler_output.find(source_path.filename().string()) == 0) {
        compiler_output.erase(0, source_path.filename().string().length());
        if (starts_with(compiler_output, "\r")) {
            compiler_output.erase(0, 1);
        }
        if (starts_with(compiler_output, "\n")) {
            compiler_output.erase(0, 1);
        }
    }

    // Log a compiler failure
    if (!compiled_okay) {
        std::string_view compilation_failure_msg
            = compile.is_syntax_only ? "Syntax check failed" : "Compilation failed";
        bpt_log(error, "{}: .bold.cyan[{}]"_styled, compilation_failure_msg, source_path.string());
        bpt_log(error,
                "Subcommand .bold.red[FAILED] [Exited {}]: .bold.yellow[{}]\n{}"_styled,
                compile_retc,
                quote_command(compile.command.command),
                compiler_output);
        if (compile_signal) {
            bpt_log(error, "Process exited via signal {}", compile_signal);
        }
        /// XXX: Use different error based on if a syntax-only check failed
        throw_user_error<errc::compile_failure>("{} [{}]",
                                                compilation_failure_msg,
                                                source_path.string());
    }

    // Print any compiler output, sans whitespace
    if (!bpt::trim_view(compiler_output).empty()) {
        bpt_log(warn,
                "While compiling file .bold.cyan[{}] [.bold.yellow[{}]]:\n{}"_styled,
                source_path.string(),
                quote_command(compile.command.command),
                compiler_output);
    }

    // We'll only get here if the compilation was successful, otherwise we throw
    assert(compiled_okay);
    return ret_deps_info;
}

/**
 * Determine if the given compile command should actually be executed based on
 * the dependency information we have recorded in the database.
 */
compile_ticket mk_compile_ticket(const compile_file_plan& plan, build_env_ref env) {
    compile_ticket ret{.plan             = plan,
                       .command          = plan.generate_compile_command(env),
                       .object_file_path = plan.calc_object_file_path(env),
                       .needs_recompile  = false,
                       .prior_command    = {},
                       .is_syntax_only   = plan.rules().syntax_only()};

    auto rb_info = get_prior_compilation(env.db, ret.object_file_path);
    if (!rb_info) {
        bpt_log(trace, "Compile {}: No recorded compilation info", plan.source_path().string());
        ret.needs_recompile = true;
    } else if (!fs::exists(ret.object_file_path) && !ret.is_syntax_only) {
        bpt_log(trace, "Compile {}: Output does not exist", plan.source_path().string());
        // The output file simply doesn't exist. We have to recompile, of course.
        ret.needs_recompile = true;
    } else if (!rb_info->newer_inputs.empty()) {
        // Inputs to this file have changed from a prior execution.
        bpt_log(trace,
                "Recompile {}: Inputs have changed (or no input information)",
                plan.source_path().string());
        for (auto& in : rb_info->newer_inputs) {
            bpt_log(trace, "  - Newer input: [{}]", in.string());
        }
        ret.needs_recompile = true;
    } else if (quote_command(ret.command.command) != rb_info->previous_command.quoted_command) {
        bpt_log(trace, "Recompile {}: Compile command has changed", plan.source_path().string());
        // The command used to generate the output is new
        ret.needs_recompile = true;
    } else {
        // Nope. This file is up-to-date.
        bpt_log(debug,
                "Skip compilation of {} (Result is up-to-date)",
                plan.source_path().string());
    }
    if (rb_info) {
        ret.prior_command = rb_info->previous_command;
    }
    return ret;
}

}  // namespace

bool bpt::detail::compile_all(const ref_vector<const compile_file_plan>& compiles,
                              build_env_ref                              env,
                              int                                        njobs) {
    auto each_realized =  //
        compiles
        // Convert each _plan_ into a concrete object for compiler invocation.
        | views::transform([&](auto&& plan) { return mk_compile_ticket(plan, env); })
        // Convert to to a real vector so we can ask its size.
        | ranges::to_vector;

    auto n_to_compile = static_cast<std::size_t>(
        ranges::count_if(each_realized, &compile_ticket::needs_recompile));

    // Keep a counter to display progress to the user.
    const auto      max_digits = fmt::format("{}", n_to_compile).size();
    compile_counter counter{.max = n_to_compile, .max_digits = max_digits};

    // Ass we execute, accumulate new dependency information from successful compilations
    std::vector<file_deps_info> all_new_deps;
    std::mutex                  mut;
    // Do it!
    auto okay = parallel_run(each_realized, njobs, [&](const compile_ticket& tkt) {
        auto new_dep = handle_compilation(tkt, env, counter);
        if (new_dep) {
            std::unique_lock lk{mut};
            all_new_deps.push_back(std::move(*new_dep));
        }
    });

    // Update compile dependency information
    bpt::stopwatch update_timer;
    auto           tr = env.db.transaction();
    for (auto& info : all_new_deps) {
        bpt_log(trace, "Update dependency info on {}", info.output.string());
        update_deps_info(neo::into(env.db), info);
    }
    bpt_log(debug, "Dependency update took {:L}ms", update_timer.elapsed_ms().count());

    cancellation_point();
    // Return whether or not there were any failures.
    return okay;
}
