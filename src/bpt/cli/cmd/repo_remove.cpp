#include "../options.hpp"

#include <bpt/crs/info/pkg_id.hpp>
#include <bpt/crs/repo.hpp>
#include <bpt/error/marker.hpp>
#include <bpt/error/try_catch.hpp>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <iostream>

namespace bpt::cli::cmd {

int repo_remove(const options& opts) {
    auto repo = bpt::crs::repository::open_existing(opts.repo.repo_dir);
    for (auto pkg : opts.repo.remove.pkgs) {
        auto pkg_id = bpt::crs::pkg_id::parse(pkg);
        /// We only need the name and version info to do the removal
        bpt::crs::package_info meta;
        meta.id = pkg_id;
        // Zero to remove all package revisions
        meta.id.revision = 0;
        repo.remove_pkg(meta);
    }
    return 0;
}

}  // namespace bpt::cli::cmd
