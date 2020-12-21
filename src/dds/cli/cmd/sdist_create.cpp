#include "../options.hpp"

#include <dds/error/errors.hpp>
#include <dds/sdist/dist.hpp>

#include <boost/leaf/common.hpp>
#include <boost/leaf/handle_exception.hpp>
#include <fmt/core.h>

namespace dds::cli::cmd {

int sdist_create(const options& opts) {
    dds::sdist_params params{
        .project_dir   = opts.project_dir,
        .dest_path     = {},
        .force         = opts.if_exists == if_exists::replace,
        .include_apps  = true,
        .include_tests = true,
    };
    return boost::leaf::try_catch(
        [&] {
            auto pkg_man = package_manifest::load_from_directory(params.project_dir).value();
            auto default_filename = fmt::format("{}.tar.gz", pkg_man.id.to_string());
            auto filepath         = opts.out_path.value_or(fs::current_path() / default_filename);
            create_sdist_targz(filepath, params);
            return 0;
        },
        [&](boost::leaf::bad_result, e_missing_file missing, e_human_message msg) {
            dds_log(error,
                    "A required file is missing for creating a source distribution for [{}]",
                    params.project_dir.string());
            dds_log(error, "Error: {}", msg.value);
            dds_log(error, "Missing file: {}", missing.path.string());
            return 1;
        },
        [&](std::error_code ec, e_human_message msg, boost::leaf::e_file_name file) {
            dds_log(error, "Error: {}", msg.value);
            dds_log(error, "Failed to access file [{}]: {}", file.value, ec.message());
            return 1;
        },
        [&](std::error_code ec, e_human_message msg) {
            dds_log(error, "Unexpected error: {}: {}", msg.value, ec.message());
            return 1;
        },
        [&](boost::leaf::bad_result, std::errc ec) {
            dds_log(error,
                    "Failed to create source distribution from directory [{}]: {}",
                    params.project_dir.string(),
                    std::generic_category().message(int(ec)));
            return 1;
        });
}

}  // namespace dds::cli::cmd
