#include "../options.hpp"

#include <dds/error/errors.hpp>
#include <dds/sdist/dist.hpp>

#include <boost/leaf.hpp>
#include <fansi/styled.hpp>
#include <fmt/core.h>
#include <neo/assert.hpp>

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
            auto sd               = sdist::from_directory(params.project_dir);
            auto default_filename = fmt::format("{}.tar.gz", sd.id().to_string());
            auto filepath         = opts.out_path.value_or(fs::current_path() / default_filename);
            create_sdist_targz(filepath, params);
            dds_log(info,
                    "Created source distribution archive: .bold.cyan[{}]"_styled,
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
                    "Failed to access file [.bold.red[{}]]: .br.yellow[{}]"_styled,
                    file.value,
                    ec.message());
            write_error_marker("failed-package-json5-scan");
            return 1;
        },
        [&](boost::leaf::catch_<user_error<errc::sdist_exists>> exc) -> int {
            if (opts.if_exists == if_exists::ignore) {
                // Satisfy the 'ignore' semantics by returning a success exit code, but still warn
                // the user to let them know what happened.
                dds_log(warn, "{}", exc.matched.what());
                return 0;
            }
            // If if_exists::replace, we wouldn't be here (not an error). Thus, since it's not
            // if_exists::ignore, it must be if_exists::fail here.
            neo_assert_always(invariant,
                              opts.if_exists == if_exists::fail,
                              "Unhandled value for if_exists");

            write_error_marker("sdist-already-exists");
            // rethrow; the default handling works.
            throw;
        });
}

}  // namespace dds::cli::cmd
