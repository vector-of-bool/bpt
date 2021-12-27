#include "../options.hpp"

#include <dds/crs/cache.hpp>
#include <dds/crs/dependency.hpp>
#include <dds/crs/repo.hpp>
#include <dds/deps.hpp>
#include <dds/error/try_catch.hpp>
#include <dds/project/dependency.hpp>
#include <dds/solve/solve2.hpp>
#include <dds/util/log.hpp>
#include <dds/util/url.hpp>

#include <fansi/styled.hpp>

#include <ranges>

using namespace fansi::literals;

namespace dds::cli::cmd {

static int _pkg_solve(const options& opts) {
    auto cache
        = dds::crs::cache::open(opts.crs_cache_dir.value_or(dds::crs::cache::default_path()));
    auto& meta_db = cache.metadata_db();
    for (auto& r : opts.use_repos) {
        auto url = dds::guess_url_from_string(r);
        meta_db.sync_remote(url);
        meta_db.enable_remote(url);
    }

    auto deps =              //
        opts.pkg.solve.reqs  //
        | std::views::transform([](std::string_view s) -> crs::dependency {
              return project_dependency::from_shorthand_string(s).as_crs_dependency();
          });

    auto sln = dds::solve2(meta_db, deps);
    for (auto&& pkg : sln) {
        dds_log(info, "Require: {}@{}~{}", pkg.name.str, pkg.version.to_string(), pkg.meta_version);
    }
    return 0;
}

int pkg_solve(const options& opts) {
    return dds_leaf_try { return _pkg_solve(opts); }
    dds_leaf_catch(e_dependency_solve_failure, e_dependency_solve_failure_explanation explain) {
        dds_log(error,
                "No solution is possible with the known package information: \n{}"_styled,
                explain.value);
        return false;
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
