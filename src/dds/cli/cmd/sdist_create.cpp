#include "../options.hpp"

#include <dds/error/errors.hpp>
#include <dds/sdist/dist.hpp>

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
    auto pkg_man = package_manifest::load_from_directory(params.project_dir);
    if (!pkg_man) {
        dds::throw_user_error<
            errc::invalid_pkg_filesystem>("The source root at [{}] is not a valid dds source root",
                                          params.project_dir.string());
    }
    auto default_filename = fmt::format("{}.tar.gz", pkg_man->id.to_string());
    auto filepath         = opts.out_path.value_or(fs::current_path() / default_filename);
    create_sdist_targz(filepath, params);
    return 0;
}

}  // namespace dds::cli::cmd
