#include "../options.hpp"

#include <dds/error/errors.hpp>
#include <dds/sdist/dist.hpp>

#include <boost/leaf/common.hpp>
#include <boost/leaf/handle_exception.hpp>
#include <fansi/styled.hpp>
#include <fmt/core.h>

using namespace fansi::literals;

namespace dds::cli::cmd {

int pkg_create(const options& opts) {
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
            dds_log(info,
                    "Created source dirtribution archive: .bold.cyan[{}]"_styled,
                    filepath.string());
            return 0;
        },
        [&](boost::leaf::bad_result, e_missing_file missing, e_human_message msg) {
            dds_log(
                error,
                "A required file is missing for creating a source distribution for [.bold.yellow[{}]]"_styled,
                params.project_dir.string());
            dds_log(error, "Error: .bold.yellow[{}]"_styled, msg.value);
            dds_log(error, "Missing file: .bold.red[{}]"_styled, missing.path.string());
            write_error_marker("no-package-json5");
            return 1;
        },
        [&](std::error_code ec, e_human_message msg, boost::leaf::e_file_name file) {
            dds_log(error, "Error: .bold.red[{}]"_styled, msg.value);
            dds_log(error,
                    "Failed to access file [.bold.red[{}]]: .br.yellow[{}]",
                    file.value,
                    ec.message());
            write_error_marker("failed-package-json5-scan");
            return 1;
        });
}

}  // namespace dds::cli::cmd
