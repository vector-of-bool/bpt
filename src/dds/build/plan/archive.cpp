#include "./archive.hpp"

#include <dds/error/errors.hpp>
#include <dds/proc.hpp>
#include <dds/util/log.hpp>
#include <dds/util/time.hpp>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

using namespace dds;

fs::path create_archive_plan::calc_archive_file_path(const toolchain& tc) const noexcept {
    return _subdir / fmt::format("{}{}{}", "lib", _name, tc.archive_suffix());
}

void create_archive_plan::archive(const build_env& env) const {
    // Convert the file compilation plans into the paths to their respective object files.
    const auto objects =  //
        _compile_files    //
        | ranges::views::transform([&](auto&& cf) { return cf.calc_object_file_path(env); })
        | ranges::to_vector  //
        ;
    // Build up the archive command
    archive_spec ar;

    auto ar_cwd    = env.output_root;
    ar.input_files = std::move(objects);
    ar.out_path    = env.output_root / calc_archive_file_path(env.toolchain);
    auto ar_cmd    = env.toolchain.create_archive_command(ar, ar_cwd, env.knobs);

    // `out_relpath` is purely for the benefit of the user to have a short name
    // in the logs
    auto out_relpath = fs::relative(ar.out_path, env.output_root).string();

    // Different archiving tools behave differently between platforms depending on whether the
    // archive file exists. Make it uniform by simply removing the prior copy.
    if (fs::exists(ar.out_path)) {
        dds_log(debug, "Remove prior archive file [{}]", ar.out_path.string());
        fs::remove(ar.out_path);
    }

    // Ensure the parent directory exists
    fs::create_directories(ar.out_path.parent_path());

    // Do it!
    dds_log(info, "[{}] Archive: {}", _qual_name, out_relpath);
    auto&& [dur_ms, ar_res] = timed<std::chrono::milliseconds>(
        [&] { return run_proc(proc_options{.command = ar_cmd, .cwd = ar_cwd}); });
    dds_log(info, "[{}] Archive: {} - {:L}ms", _qual_name, out_relpath, dur_ms.count());

    // Check, log, and throw
    if (!ar_res.okay()) {
        dds_log(error,
                "Creating static library archive [{}] failed for '{}'",
                out_relpath,
                _qual_name);
        dds_log(error, "Subcommand FAILED: {}\n{}", quote_command(ar_cmd), ar_res.output);
        throw_external_error<
            errc::archive_failure>("Creating static library archive [{}] failed for '{}'",
                                   out_relpath,
                                   _qual_name);
    }
}
