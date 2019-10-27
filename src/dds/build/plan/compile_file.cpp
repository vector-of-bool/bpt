#include "./compile_file.hpp"

#include <dds/proc.hpp>
#include <dds/util/algo.hpp>
#include <dds/util/signal.hpp>

#include <spdlog/spdlog.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

using namespace dds;

void compile_file_plan::compile(const build_env& env) const {
    const auto obj_path = calc_object_file_path(env);
    fs::create_directories(obj_path.parent_path());

    spdlog::info("[{}] Compile: {}",
                 _qualifier,
                 fs::relative(_source.path, _source.basis_path).string());
    auto start_time = std::chrono::steady_clock::now();

    compile_file_spec spec{_source.path, obj_path};
    spec.enable_warnings = _rules.enable_warnings();

    extend(spec.include_dirs, _rules.include_dirs());
    extend(spec.definitions, _rules.defs());

    auto cmd         = env.toolchain.create_compile_command(spec);
    auto compile_res = run_proc(cmd);

    auto end_time = std::chrono::steady_clock::now();
    auto dur_ms   = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    spdlog::info("[{}] Compile: {} - {:n}ms",
                 _qualifier,
                 fs::relative(_source.path, _source.basis_path).string(),
                 dur_ms.count());

    if (!compile_res.okay()) {
        spdlog::error("Compilation failed: {}", _source.path.string());
        spdlog::error("Subcommand FAILED: {}\n{}", quote_command(cmd), compile_res.output);
        throw compile_failure(fmt::format("Compilation failed for {}", _source.path.string()));
    }

    // MSVC prints the filename of the source file. Dunno why, but they do.
    if (compile_res.output.find(spec.source_path.filename().string() + "\r\n") == 0) {
        compile_res.output.erase(0, spec.source_path.filename().string().length() + 2);
    }

    if (!compile_res.output.empty()) {
        spdlog::warn("While compiling file {} [{}]:\n{}",
                     spec.source_path.string(),
                     quote_command(cmd),
                     compile_res.output);
    }
}

fs::path compile_file_plan::calc_object_file_path(const build_env& env) const noexcept {
    auto relpath = fs::relative(_source.path, _source.basis_path);
    auto ret     = env.output_root / _subdir / relpath;
    ret.replace_filename(relpath.filename().string() + env.toolchain.object_suffix());
    return ret;
}
