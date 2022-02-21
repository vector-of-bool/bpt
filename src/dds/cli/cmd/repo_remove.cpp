#include "../options.hpp"

#include <dds/crs/info/pkg_id.hpp>
#include <dds/crs/repo.hpp>
#include <dds/error/marker.hpp>
#include <dds/error/try_catch.hpp>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <iostream>

namespace dds::cli::cmd {

int repo_remove(const options& opts) {
    auto repo = dds::crs::repository::open_existing(opts.repo.repo_dir);
    for (auto pkg : opts.repo.remove.pkgs) {
        auto pkg_id = dds::crs::pkg_id::parse(pkg);
        /// We only need the name and version info to do the removal
        dds::crs::package_info meta;
        meta.id = pkg_id;
        // Zero to remove all package revisions
        meta.id.revision = 0;
        repo.remove_pkg(meta);
    }
    return 0;
}

}  // namespace dds::cli::cmd
