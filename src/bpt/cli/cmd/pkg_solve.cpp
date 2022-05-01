#include "../options.hpp"

#include "./cache_util.hpp"

#include <bpt/crs/cache.hpp>
#include <bpt/crs/info/dependency.hpp>
#include <bpt/crs/repo.hpp>
#include <bpt/deps.hpp>
#include <bpt/error/marker.hpp>
#include <bpt/error/try_catch.hpp>
#include <bpt/project/dependency.hpp>
#include <bpt/solve/solve.hpp>
#include <bpt/util/log.hpp>
#include <bpt/util/url.hpp>

#include <fansi/styled.hpp>

#include <ranges>

using namespace fansi::literals;

namespace bpt::cli::cmd {

static int _pkg_solve(const options& opts) {
    auto cache = open_ready_cache(opts);

    auto deps =              //
        opts.pkg.solve.reqs  //
        | std::views::transform([](std::string_view s) -> crs::dependency {
              return project_dependency::from_shorthand_string(s).as_crs_dependency();
          });

    auto sln = bpt::solve(cache.db(), deps);
    for (auto&& pkg : sln) {
        bpt_log(info, "Require: {}", pkg.to_string());
    }
    return 0;
}

int pkg_solve(const options& opts) {
    return bpt_leaf_try { return _pkg_solve(opts); }
    bpt_leaf_catch(e_dependency_solve_failure,
                   e_dependency_solve_failure_explanation       explain,
                   const std::vector<e_nonesuch_package>*       missing_pkgs,
                   const std::vector<e_nonesuch_using_library>* missing_libs) {
        bpt_log(error,
                "No solution is possible with the known package information: \n{}"_styled,
                explain.value);
        if (missing_pkgs) {
            for (auto& missing : *missing_pkgs) {
                missing.log_error(
                    "Direct requirement on '.bold.red[{}]' does not name an existing package in any enabled repositories"_styled);
            }
        }
        if (missing_libs) {
            for (auto& lib : *missing_libs) {
                bpt_log(
                    error,
                    "There is no available version of .bold.yellow[{}] that contains a library named \".bold.red[{}]\""_styled,
                    lib.pkg_name.str,
                    lib.lib.given);
                if (lib.lib.nearest) {
                    bpt_log(error,
                            "  (Did you mean \".bold.yellow[{}]\"?)"_styled,
                            *lib.lib.nearest);
                }
            }
        }
        write_error_marker("no-dependency-solution");
        return 1;
    };
}

}  // namespace bpt::cli::cmd
