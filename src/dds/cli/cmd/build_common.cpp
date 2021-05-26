#include "./build_common.hpp"

#include <dds/error/errors.hpp>
#include <dds/pkg/cache.hpp>
#include <dds/pkg/db.hpp>
#include <dds/pkg/get/get.hpp>
#include <dds/util/algo.hpp>

#include <boost/leaf/handle_exception.hpp>

using namespace dds;

builder dds::cli::create_project_builder(const dds::cli::options& opts) {
    sdist_build_params main_params = {
        .subdir          = "",
        .build_tests     = opts.build.want_tests,
        .run_tests       = opts.build.want_tests,
        .build_apps      = opts.build.want_apps,
        .enable_warnings = !opts.disable_warnings,
    };

    auto man
        = value_or(package_manifest::load_from_directory(opts.project_dir), package_manifest{});
    auto cat_path  = opts.pkg_db_dir.value_or(pkg_db::default_path());
    auto repo_path = opts.pkg_cache_dir.value_or(pkg_cache::default_local_path());

    builder builder;
    if (!opts.build.lm_index.has_value()) {
        auto cat = pkg_db::open(cat_path);
        // Build the dependencies
        pkg_cache::with_cache(  //
            repo_path,
            pkg_cache_flags::write_lock | pkg_cache_flags::create_if_absent,
            [&](pkg_cache repo) {
                // Download dependencies
                auto deps = repo.solve(  //
                    man.dependencies,
                    cat,
                    opts.build.want_tests ? dds::with_test_deps : dds::without_test_deps,
                    opts.build.want_apps ? dds::with_app_deps : dds::without_app_deps);
                get_all(deps, repo, cat);

                for (const pkg_id& pk : deps) {
                    auto sdist_ptr = repo.find(pk);
                    assert(sdist_ptr);
                    sdist_build_params deps_params;
                    deps_params.subdir = fs::path("_deps") / sdist_ptr->manifest.id.to_string();
                    builder.add(*sdist_ptr, deps_params);
                }
            });
    }
    builder.add(sdist{std::move(man), opts.project_dir}, main_params);
    return builder;
}

int dds::cli::handle_build_error(std::function<int()> fn) {
    return boost::leaf::try_catch(  //
        [&] {
            try {
                return fn();
            } catch (...) {
                capture_exception();
            }
        },
        [](user_error<errc::compile_failure>) -> int {
            write_error_marker("compile-failed");
            throw;
        },
        [](user_error<errc::test_failure> exc) {
            write_error_marker("build-failed-test-failed");
            dds_log(error, "{}", exc.what());
            dds_log(error, "{}", exc.explanation());
            dds_log(error, "Refer: {}", exc.error_reference());
            return 1;
        });
}
