#include "./exe.hpp"

#include <dds/build/plan/library.hpp>
#include <dds/error/errors.hpp>
#include <dds/proc.hpp>
#include <dds/util/algo.hpp>
#include <dds/util/time.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>

using namespace dds;

fs::path link_executable_plan::calc_executable_path(build_env_ref env) const noexcept {
    return env.output_root / _out_subdir / (_name + env.toolchain.executable_suffix());
}

void link_executable_plan::link(build_env_ref env, const library_plan& lib) const {
    // Build up the link command
    link_exe_spec spec;
    spec.output = calc_executable_path(env);
    spec.inputs = _input_libs;
    for (const lm::usage& links : _links) {
        extend(spec.inputs, env.ureqs.link_paths(links));
    }
    if (lib.create_archive()) {
        // The associated library has compiled components. Add the static library a as a linker
        // input
        spec.inputs.push_back(env.output_root
                              / lib.create_archive()->calc_archive_file_path(env.toolchain));
    }

    // The main object should be a linker input, of course.
    auto main_obj = _main_compile.calc_object_file_path(env);
    spec.inputs.push_back(std::move(main_obj));

    // Linker inputs are order-dependent in some cases. The top-most input should appear first, and
    // its dependencies should appear later. Because of the way inputs were generated, they appear
    // sorted with the dependencies coming earlier than the dependees. We can simply reverse the
    // order and linking will work.
    std::reverse(spec.inputs.begin(), spec.inputs.end());

    // Do it!
    const auto link_command = env.toolchain.create_link_executable_command(spec);
    fs::create_directories(spec.output.parent_path());
    auto msg = fmt::format("[{}] Link: {:30}",
                           lib.name(),
                           fs::relative(spec.output, env.output_root).string());
    spdlog::info(msg);
    auto [dur_ms, proc_res]
        = timed<std::chrono::milliseconds>([&] { return run_proc(link_command); });
    spdlog::info("{} - {:>6n}ms", msg, dur_ms.count());

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
    auto msg = fmt::format("Run test: {:30}", fs::relative(exe_path, env.output_root).string());
    spdlog::info(msg);
    auto&& [dur, res]
        = timed<std::chrono::microseconds>([&] { return run_proc({exe_path.string()}); });
    if (res.okay()) {
        spdlog::info("{} - PASSED - {:>9n}μs", msg, dur.count());
        return std::nullopt;
    } else {
        spdlog::error("{} - FAILED - {:>9n}μs [exited {}]", msg, dur.count(), res.retc);
        test_failure f;
        f.executable_path = exe_path;
        f.output          = res.output;
        f.retc            = res.retc;
        return f;
    }
}
