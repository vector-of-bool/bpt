#include "./archive.hpp"

#include <bpt/error/doc_ref.hpp>
#include <bpt/error/errors.hpp>
#include <bpt/util/log.hpp>
#include <bpt/util/proc.hpp>
#include <bpt/util/time.hpp>

#include <boost/leaf/exception.hpp>
#include <fansi/styled.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

using namespace bpt;
using namespace fansi::literals;

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
        bpt_log(debug, "Remove prior archive file [{}]", ar.out_path.string());
        fs::remove(ar.out_path);
    }

    // Ensure the parent directory exists
    fs::create_directories(ar.out_path.parent_path());

    // Do it!
    bpt_log(info, "[{}] Archive: {}", _qual_name, out_relpath);
    auto&& [dur_ms, ar_res] = timed<std::chrono::milliseconds>(
        [&] { return run_proc(proc_options{.command = ar_cmd, .cwd = ar_cwd}); });
    bpt_log(info, "[{}] Archive: {} - {:L}ms", _qual_name, out_relpath, dur_ms.count());

    // Check, log, and throw
    if (!ar_res.okay()) {
        bpt_log(error,
                "Creating static library archive [{}] failed for '{}'",
                out_relpath,
                _qual_name);
        bpt_log(error,
                "Subcommand FAILED: .bold.yellow[{}]\n{}"_styled,
                quote_command(ar_cmd),
                ar_res.output);
        BOOST_LEAF_THROW_EXCEPTION(make_external_error<errc::archive_failure>(
                                       "Creating static library archive [{}] failed for '{}'",
                                       out_relpath,
                                       _qual_name),
                                   BPT_ERR_REF("archive-failure"));
    }
}
