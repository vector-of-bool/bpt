#include "./exe.hpp"

#include <dds/build/plan/library.hpp>
#include <dds/proc.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>

using namespace dds;

fs::path link_executable_plan::calc_executable_path(build_env_ref env) const noexcept {
    return env.output_root / _out_subdir / (_name + env.toolchain.executable_suffix());
}

void link_executable_plan::link(build_env_ref env, const library_plan& lib) const {
    const auto out_path = calc_executable_path(env);

    link_exe_spec spec;
    spec.output = out_path;
    spec.inputs = _input_libs;
    if (lib.create_archive()) {
        spec.inputs.push_back(lib.create_archive()->calc_archive_file_path(env));
    }

    auto main_obj = _main_compile.calc_object_file_path(env);
    spec.inputs.push_back(std::move(main_obj));
    std::reverse(spec.inputs.begin(), spec.inputs.end());

    const auto link_command = env.toolchain.create_link_executable_command(spec);
    spdlog::info("Linking executable: {}", fs::relative(spec.output, env.output_root).string());
    fs::create_directories(out_path.parent_path());
    auto proc_res = run_proc(link_command);
    if (!proc_res.okay()) {
        throw compile_failure(
            fmt::format("Failed to link test executable '{}'. Link command [{}] returned {}:\n{}",
                        spec.output.string(),
                        quote_command(link_command),
                        proc_res.retc,
                        proc_res.output));
    }
}
