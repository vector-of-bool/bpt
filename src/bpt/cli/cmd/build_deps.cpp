#include "../options.hpp"

#include "./build_common.hpp"
#include "./cache_util.hpp"

#include <bpt/build/builder.hpp>
#include <bpt/build/params.hpp>
#include <bpt/crs/cache.hpp>
#include <bpt/deps.hpp>
#include <bpt/project/dependency.hpp>
#include <bpt/solve/solve.hpp>

#include <neo/ranges.hpp>
#include <neo/tl.hpp>
#include <range/v3/action/join.hpp>
#include <range/v3/action/push_back.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/transform.hpp>

namespace bpt::cli::cmd {

static int _build_deps(const options& opts) {
    auto cache = open_ready_cache(opts);

    bpt::build_params params{
        .out_root          = opts.out_path.value_or(fs::current_path() / "_deps"),
        .existing_lm_index = {},
        .emit_lmi          = opts.build.lm_index.value_or("INDEX.lmi"),
        .emit_cmake        = opts.build_deps.cmake_file,
        .tweaks_dir        = opts.build.tweaks_dir,
        .toolchain         = opts.load_toolchain(),
        .parallel_jobs     = opts.jobs,
    };

    bpt::builder            builder;
    bpt::sdist_build_params sdist_params;

    neo::ranges::range_of<crs::dependency> auto file_deps
        = opts.build_deps.deps_files  //
        | ranges::views::transform([&](auto dep_fpath) {
              bpt_log(info, "Reading deps from {}", dep_fpath.string());
              bpt::dependency_manifest depman = bpt::dependency_manifest::from_file(dep_fpath);
              return depman.dependencies
                  | std::views::transform(&project_dependency::as_crs_dependency) | neo::to_vector;
          })
        | ranges::actions::join;

    neo::ranges::range_of<crs::dependency> auto cli_deps
        = std::views::transform(opts.build_deps.deps,
                                NEO_TL(bpt::project_dependency::from_shorthand_string(_1)
                                           .as_crs_dependency()));

    neo::ranges::range_of<crs::dependency> auto all_deps
        = ranges::views::concat(file_deps, cli_deps);

    auto sln = bpt::solve(cache.db(), all_deps);
    for (auto&& pkg : sln) {
        fetch_cache_load_dependency(cache,
                                    pkg,
                                    true /* Build all libraries in the dependency */,
                                    builder,
                                    ".");
    }

    builder.build(params);
    return 0;
}

int build_deps(const options& opts) {
    return handle_build_error([&] { return _build_deps(opts); });
}

}  // namespace bpt::cli::cmd
