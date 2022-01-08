#include "../options.hpp"

#include "./build_common.hpp"

#include <dds/crs/cache.hpp>
#include <dds/crs/meta/dependency.hpp>
#include <dds/crs/repo.hpp>
#include <dds/deps.hpp>
#include <dds/error/marker.hpp>
#include <dds/error/try_catch.hpp>
#include <dds/project/dependency.hpp>
#include <dds/solve/solve.hpp>
#include <dds/util/log.hpp>
#include <dds/util/url.hpp>

#include <fansi/styled.hpp>

#include <ranges>

using namespace fansi::literals;

namespace dds::cli::cmd {

static int _pkg_solve(const options& opts) {
    auto cache = open_ready_cache(opts);

    auto deps =              //
        opts.pkg.solve.reqs  //
        | std::views::transform([](std::string_view s) -> crs::dependency {
              return project_dependency::from_shorthand_string(s).as_crs_dependency();
          });

    auto sln = dds::solve(cache.metadata_db(), deps);
    for (auto&& pkg : sln) {
        dds_log(info, "Require: {}", pkg.to_string());
    }
    return 0;
}

int pkg_solve(const options& opts) {
    return dds_leaf_try { return _pkg_solve(opts); }
    dds_leaf_catch(e_dependency_solve_failure, e_dependency_solve_failure_explanation explain) {
        dds_log(error,
                "No solution is possible with the known package information: \n{}"_styled,
                explain.value);
        write_error_marker("no-dependency-solution");
        return 1;
    }
    dds_leaf_catch(e_usage_namespace_mismatch,
                   crs::dependency   dep,
                   crs::package_meta package,
                   lm::usage         usage) {
        dds_log(
            error,
            "Dependency on .br.cyan[{}] uses library .br.yellow[{}], but .br.red[{}] does not match the package namespace of .br.yellow[{}]"_styled,
            dep.name.str,
            usage,
            usage.namespace_,
            package.namespace_.str);
        return 1;
    };
}

}  // namespace dds::cli::cmd
