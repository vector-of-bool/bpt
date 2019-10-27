#include "./archive.hpp"

#include <dds/proc.hpp>

#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

using namespace dds;

fs::path create_archive_plan::calc_archive_file_path(const build_env& env) const noexcept {
    return env.output_root / fmt::format("{}{}{}", "lib", _name, env.toolchain.archive_suffix());
}

void create_archive_plan::archive(const build_env& env) const {
    const auto objects =  //
        _compile_files    //
        | ranges::views::transform([&](auto&& cf) { return cf.calc_object_file_path(env); })
        | ranges::to_vector  //
        ;
    archive_spec ar;
    ar.input_files   = std::move(objects);
    ar.out_path      = calc_archive_file_path(env);
    auto ar_cmd      = env.toolchain.create_archive_command(ar);
    auto out_relpath = fs::relative(ar.out_path, env.output_root).string();
    if (fs::exists(ar.out_path)) {
        fs::remove(ar.out_path);
    }

    spdlog::info("[{}] Archive: {}", _name, out_relpath);
    auto start_time = std::chrono::steady_clock::now();
    auto ar_res     = run_proc(ar_cmd);
    auto end_time   = std::chrono::steady_clock::now();
    auto dur_ms     = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    spdlog::info("[{}] Archive: {} - {:n}ms", _name, out_relpath, dur_ms.count());

    if (!ar_res.okay()) {
        spdlog::error("Creating static library archive failed: {}", out_relpath);
        spdlog::error("Subcommand FAILED: {}\n{}", quote_command(ar_cmd), ar_res.output);
        throw std::runtime_error(
            fmt::format("Creating archive [{}] failed for '{}'", out_relpath, _name));
    }
}
