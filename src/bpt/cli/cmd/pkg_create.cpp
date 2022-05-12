#include "../options.hpp"

#include <bpt/error/errors.hpp>
#include <bpt/error/marker.hpp>
#include <bpt/error/try_catch.hpp>
#include <bpt/sdist/dist.hpp>
#include <bpt/sdist/error.hpp>
#include <bpt/util/json5/error.hpp>

#include <boost/leaf.hpp>
#include <fansi/styled.hpp>
#include <fmt/core.h>
#include <neo/assert.hpp>

using namespace fansi::literals;

namespace bpt::cli::cmd {

int pkg_create(const options& opts) {
    bpt::sdist_params params{
        .project_dir = opts.absolute_project_dir_path(),
        .dest_path   = {},
        .force       = opts.if_exists == if_exists::replace,
        .revision    = opts.pkg.create.revision,
    };
    if (params.revision < 1) {
        bpt_log(
            error,
            ".bold.cyan[--revision] must be greater than or equal to one '1' (Got .bold.red[{}])"_styled,
            params.revision);
        return 1;
    }
    return bpt_leaf_try {
        auto sd               = sdist::from_directory(params.project_dir);
        sd.pkg.id.revision    = params.revision;
        auto default_filename = fmt::format("{}.tar.gz", sd.pkg.id.to_string());
        auto filepath         = opts.out_path.value_or(fs::current_path() / default_filename);
        create_sdist_targz(filepath, params);
        bpt_log(info,
                "Created source distribution archive: .bold.cyan[{}]"_styled,
                filepath.string());
        return 0;
    }
    bpt_leaf_catch(e_sdist_from_directory dirpath,
                   e_missing_pkg_json     expect_pkg_json,
                   e_missing_project_yaml expect_proj_json5) {
        bpt_log(
            error,
            "No package or project files are presenting in the directory [.bold.red[{}]]"_styled,
            dirpath.value.string());
        bpt_log(error, "Expected [.bold.yellow[{}]]"_styled, expect_pkg_json.value.string());
        bpt_log(error, "      or [.bold.yellow[{}]]"_styled, expect_proj_json5.value.string());
        write_error_marker("no-pkg-meta-files");
        return 1;
    }
    bpt_leaf_catch(e_sdist_from_directory,
                   e_json_parse_error           error,
                   const std::filesystem::path& fpath) {
        bpt_log(error,
                "Invalid JSON/JSON5 file [.bold.yellow[{}]]: .bold.red[{}]"_styled,
                fpath.string(),
                error.value);
        write_error_marker("package-json5-parse-error");
        return 1;
    }
    bpt_leaf_catch(const std::system_error&     exc,
                   e_sdist_from_directory       dirpath,
                   std::filesystem::path const* fpath) {
        bpt_log(error,
                "Error while opening source distribution from [{}]: {}"_styled,
                dirpath.value.string(),
                exc.what());
        if (fpath) {
            bpt_log(error, "  (Failing path was [.bold.yellow[{}]])"_styled, fpath->string());
        }
        write_error_marker("sdist-open-fail-generic");
        return 1;
    }
    bpt_leaf_catch(boost::leaf::catch_<user_error<errc::sdist_exists>> exc)->int {
        if (opts.if_exists == if_exists::ignore) {
            // Satisfy the 'ignore' semantics by returning a success exit code, but still warn
            // the user to let them know what happened.
            bpt_log(warn, "{}", exc.matched.what());
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
    };
}

}  // namespace bpt::cli::cmd
