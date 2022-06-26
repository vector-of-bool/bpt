#include "../options.hpp"

#include <bpt/crs/info/pkg_id.hpp>
#include <bpt/crs/repo.hpp>
#include <bpt/error/exit.hpp>
#include <bpt/error/marker.hpp>
#include <bpt/error/try_catch.hpp>
#include <bpt/solve/solve.hpp>  // for e_nonesuch_package

#include <fansi/styled.hpp>
#include <fmt/format.h>
#include <fmt/ostream.h>

#include <iostream>

using namespace fansi::styled_literals;

namespace bpt::cli::cmd {

int repo_remove(const options& opts) {
    auto repo = bpt::crs::repository::open_existing(opts.repo.repo_dir);
    for (auto pkg : opts.repo.remove.pkgs) {
        auto pkg_id = bpt::crs::pkg_id::parse(pkg);
        bpt_log(info, "Removing .bold.yellow[{}]"_styled, pkg_id.to_string());
        /// We only need the name and version info to do the removal
        bpt::crs::package_info meta;
        meta.id = pkg_id;
        bpt_leaf_try { repo.remove_pkg(meta); }
        bpt_leaf_catch(bpt::e_nonesuch_package missing) {
            if (opts.if_missing == if_missing::ignore) {
                bpt_log(info,
                        "Ignoring non-existent package .bold.yellow[{}]"_styled,
                        missing.given);
            } else {
                missing.log_error("No such package .bold.red[{}]"_styled);
                write_error_marker("pkg-remove-nonesuch");
                bpt::throw_system_exit(1);
            }
        };
    }
    return 0;
}

}  // namespace bpt::cli::cmd
