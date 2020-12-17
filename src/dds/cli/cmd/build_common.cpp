#include "./build_common.hpp"

#include <dds/catalog/catalog.hpp>
#include <dds/catalog/get.hpp>
#include <dds/repo/repo.hpp>

using namespace dds;

builder dds::cli::create_project_builder(const dds::cli::options& opts) {
    sdist_build_params main_params = {
        .subdir          = "",
        .build_tests     = opts.build.want_tests,
        .run_tests       = opts.build.want_tests,
        .build_apps      = opts.build.want_apps,
        .enable_warnings = !opts.disable_warnings,
    };

    auto man = package_manifest::load_from_directory(opts.project_dir).value_or(package_manifest{});
    auto cat_path  = opts.pkg_db_dir.value_or(catalog::default_path());
    auto repo_path = opts.pkg_cache_dir.value_or(repository::default_local_path());

    builder builder;
    if (!opts.build.lm_index.has_value()) {
        auto cat = catalog::open(cat_path);
        // Build the dependencies
        repository::with_repository(  //
            repo_path,
            repo_flags::write_lock | repo_flags::create_if_absent,
            [&](repository repo) {
                // Download dependencies
                auto deps = repo.solve(man.dependencies, cat);
                get_all(deps, repo, cat);
                for (const package_id& pk : deps) {
                    auto sdist_ptr = repo.find(pk);
                    assert(sdist_ptr);
                    sdist_build_params deps_params;
                    deps_params.subdir = fs::path("_deps") / sdist_ptr->manifest.pkg_id.to_string();
                    builder.add(*sdist_ptr, deps_params);
                }
            });
    }
    builder.add(sdist{std::move(man), opts.project_dir}, main_params);
    return builder;
}
