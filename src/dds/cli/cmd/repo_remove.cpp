#include "../options.hpp"

#include <dds/crs/meta/pkg_id.hpp>
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
        dds::crs::package_meta meta;
        meta.name         = pkg_id.name;
        meta.version      = pkg_id.version;
        meta.meta_version = pkg_id.meta_version;
        // Zero to remove all package meta-versions
        meta.meta_version = 0;
        repo.remove_pkg(meta);
    }
    return 0;
}

}  // namespace dds::cli::cmd
