#include "./exe.hpp"

#include <dds/build/plan/library.hpp>
#include <dds/error/errors.hpp>
#include <dds/util/algo.hpp>
#include <dds/util/log.hpp>
#include <dds/util/proc.hpp>
#include <dds/util/time.hpp>

#include <fansi/styled.hpp>

#include <algorithm>
#include <chrono>

using namespace dds;
using namespace fansi::literals;

fs::path link_executable_plan::calc_executable_path(build_env_ref env) const noexcept {
    return env.output_root / _out_subdir / (_name + env.toolchain.executable_suffix());
}

void link_executable_plan::link(build_env_ref env, const library_plan& lib) const {
    // Build up the link command
    link_exe_spec spec;
    spec.output = calc_executable_path(env);

    dds_log(debug, "Performing link for {}", spec.output.string());

    // The main object should be a linker input, of course.
    auto main_obj = _main_compile.calc_object_file_path(env);
    dds_log(trace, "Add entry point object file: {}", main_obj.string());
    spec.inputs.push_back(std::move(main_obj));

    if (lib.archive_plan()) {
        // The associated library has compiled components. Add the static library a as a linker
        // input
        dds_log(trace, "Adding the library's archive as a linker input");
        spec.inputs.push_back(env.output_root
                              / lib.archive_plan()->calc_archive_file_path(env.toolchain));
    } else {
        dds_log(trace, "Executable has no corresponding archive library input");
    }

    for (const lm::usage& links : _links) {
        dds_log(trace, "  - Link with: {}/{}", links.name, links.namespace_);
        extend(spec.inputs, env.ureqs.link_paths(links));
    }

    // Do it!
    const auto link_command
        = env.toolchain.create_link_executable_command(spec, dds::fs::current_path(), env.knobs);
    fs::create_directories(spec.output.parent_path());
    auto msg = fmt::format("[{}] Link: {:30}",
                           lib.qualified_name(),
                           fs::relative(spec.output, env.output_root).string());
    dds_log(info, msg);
    auto [dur_ms, proc_res]
        = timed<std::chrono::milliseconds>([&] { return run_proc(link_command); });
    dds_log(info, "{} - {:>6L}ms", msg, dur_ms.count());

    // Check and throw if errant
    if (!proc_res.okay()) {
        throw_external_error<errc::link_failure>(
            "Failed to link executable [{}]. Link command was [{}] [Exited {}], produced "
            "output:\n{}",
            spec.output.string(),
            quote_command(link_command),
            proc_res.retc,
            proc_res.output);
    }
}

bool link_executable_plan::is_app() const noexcept {
    return _main_compile.source().kind == source_kind::app;
}

bool link_executable_plan::is_test() const noexcept {
    return _main_compile.source().kind == source_kind::test;
}

std::optional<test_failure> link_executable_plan::run_test(build_env_ref env) const {
    auto exe_path = calc_executable_path(env);
    auto msg      = fmt::format("Run test: .br.cyan[{:30}]"_styled,
                           fs::relative(exe_path, env.output_root).string());
    dds_log(info, msg);
    using namespace std::chrono_literals;
    auto&& [dur, res] = timed<std::chrono::microseconds>(
        [&] { return run_proc({.command = {exe_path.string()}, .timeout = 10s}); });

    if (res.okay()) {
        dds_log(info, "{} - .br.green[PASS] - {:>9L}μs"_styled, msg, dur.count());
        return std::nullopt;
    } else {
        auto exit_msg = fmt::format(res.signal ? "signalled {}" : "exited {}",
                                    res.signal ? res.signal : res.retc);
        auto fail_str = res.timed_out ? ".br.yellow[TIME]"_styled : ".br.red[FAIL]"_styled;
        dds_log(error, "{} - {} - {:>9L}μs [{}]", msg, fail_str, dur.count(), exit_msg);
        test_failure f;
        f.executable_path = exe_path;
        f.output          = res.output;
        f.retc            = res.retc;
        f.signal          = res.signal;
        f.timed_out       = res.timed_out;
        return f;
    }
}
