#include "../options.hpp"

#include <dds/build/builder.hpp>
#include <dds/build/params.hpp>
#include <dds/pkg/cache.hpp>
#include <dds/pkg/get/get.hpp>

#include <range/v3/action/join.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/transform.hpp>

namespace dds::cli::cmd {

int build_deps(const options& opts) {
    dds::build_params params{
        .out_root          = opts.out_path.value_or(fs::current_path() / "_deps"),
        .existing_lm_index = {},
        .emit_lmi          = opts.build.lm_index.value_or("INDEX.lmi"),
        .toolchain         = opts.load_toolchain(),
        .parallel_jobs     = opts.jobs,
    };

    dds::builder            bd;
    dds::sdist_build_params sdist_params;

    auto all_file_deps = opts.build_deps.deps_files  //
        | ranges::views::transform([&](auto dep_fpath) {
                             dds_log(info, "Reading deps from {}", dep_fpath.string());
                             return dds::dependency_manifest::from_file(dep_fpath).dependencies;
                         })
        | ranges::actions::join;

    auto cmd_deps = ranges::views::transform(opts.build_deps.deps, [&](auto dep_str) {
        return dds::dependency::parse_depends_string(dep_str);
    });

    auto all_deps = ranges::views::concat(all_file_deps, cmd_deps) | ranges::to_vector;

    auto cat = opts.open_catalog();
    dds::pkg_cache::with_cache(  //
        opts.pkg_cache_dir.value_or(pkg_cache::default_local_path()),
        dds::pkg_cache_flags::write_lock | dds::pkg_cache_flags::create_if_absent,
        [&](dds::pkg_cache repo) {
            // Download dependencies
            dds_log(info, "Loading {} dependencies", all_deps.size());
            auto deps = repo.solve(all_deps, cat);
            dds::get_all(deps, repo, cat);
            for (const dds::pkg_id& pk : deps) {
                auto sdist_ptr = repo.find(pk);
                assert(sdist_ptr);
                dds::sdist_build_params deps_params;
                deps_params.subdir = sdist_ptr->manifest.id.to_string();
                dds_log(info, "Dependency: {}", sdist_ptr->manifest.id.to_string());
                bd.add(*sdist_ptr, deps_params);
            }
        });

    bd.build(params);
    return 0;
}

}  // namespace dds::cli::cmd
