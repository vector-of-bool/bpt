#include "./build_common.hpp"

#include "./cache_util.hpp"

#include <bpt/crs/cache.hpp>
#include <bpt/crs/cache_db.hpp>
#include <bpt/crs/info/dependency.hpp>
#include <bpt/error/errors.hpp>
#include <bpt/error/marker.hpp>
#include <bpt/error/on_error.hpp>
#include <bpt/error/try_catch.hpp>
#include <bpt/project/project.hpp>
#include <bpt/sdist/error.hpp>
#include <bpt/solve/solve.hpp>
#include <bpt/util/algo.hpp>
#include <bpt/util/fs/io.hpp>

#include <boost/leaf/pred.hpp>
#include <fansi/styled.hpp>
#include <neo/ranges.hpp>
#include <neo/sqlite3/error.hpp>
#include <neo/tl.hpp>

using namespace bpt;
using namespace fansi::literals;

static void resolve_implicit_usages(crs::package_info& proj_meta, crs::package_info& dep_meta) {
    proj_meta.libraries                                                         //
        | std::views::transform(NEO_TL(_1.dependencies))                        //
        | std::views::join                                                      //
        | std::views::filter(NEO_TL(_1.name == dep_meta.id.name))               //
        | std::views::transform(NEO_TL(_1.uses))                                //
        | std::views::filter(NEO_TL(_1.template is<crs::implicit_uses_all>()))  //
        | neo::ranges::each([&](crs::dependency_uses& item) {
              item = crs::explicit_uses_list{dep_meta.libraries
                                             | std::views::transform(&bpt::crs::library_info::name)
                                             | neo::to_vector};
          });
}

builder bpt::cli::create_project_builder(const bpt::cli::options& opts) {
    sdist_build_params main_params = {
        .subdir          = "",
        .build_tests     = opts.build.want_tests,
        .run_tests       = opts.build.want_tests,
        .build_apps      = opts.build.want_apps,
        .enable_warnings = !opts.disable_warnings,
    };

    auto  cache   = open_ready_cache(opts);
    auto& meta_db = cache.db();

    sdist proj_sd = bpt_leaf_try { return sdist::from_directory(opts.absolute_project_dir_path()); }
    bpt_leaf_catch(bpt::e_missing_pkg_json, bpt::e_missing_project_yaml) {
        crs::package_info default_meta;
        default_meta.id.name.str = "anon";
        default_meta.id.revision = 0;
        crs::library_info default_library;
        default_library.name.str = "anon";
        default_meta.libraries.push_back(default_library);
        return sdist{std::move(default_meta), opts.absolute_project_dir_path()};
    };

    builder builder;
    if (!opts.build.lm_index.has_value()) {
        auto crs_deps = proj_sd.pkg.libraries | std::views::transform(NEO_TL(_1.dependencies))
            | std::views::join | neo::to_vector;

        if (opts.build.want_tests) {
            extend(crs_deps,
                   proj_sd.pkg.libraries | std::views::transform(NEO_TL(_1.test_dependencies))
                       | std::views::join);
        }

        auto sln = bpt::solve(meta_db, crs_deps);
        for (auto&& pkg : sln) {
            auto dep_meta = fetch_cache_load_dependency(cache, pkg, builder, "_deps");
            resolve_implicit_usages(proj_sd.pkg, dep_meta);
        }
    }
    builder.add(proj_sd, main_params);
    return builder;
}

crs::package_info bpt::cli::fetch_cache_load_dependency(crs::cache&        cache,
                                                        crs::pkg_id const& pkg,
                                                        bpt::builder&      builder,
                                                        path_ref           subdir_base) {
    bpt_log(debug, "Loading package '{}' for build", pkg.to_string());
    auto local_dir        = cache.prefetch(pkg);
    auto pkg_json_path    = local_dir / "pkg.json";
    auto pkg_json_content = bpt::read_file(pkg_json_path);
    BPT_E_SCOPE(crs::e_pkg_json_path{pkg_json_path});
    auto crs_meta = crs::package_info::from_json_str(pkg_json_content);

    bpt::sdist         sd{crs_meta, local_dir};
    sdist_build_params params;
    params.subdir = subdir_base / sd.pkg.id.to_string();
    builder.add(sd, params);
    return crs_meta;
}

int bpt::cli::handle_build_error(std::function<int()> fn) {
    return bpt_leaf_try { return fn(); }
    bpt_leaf_catch(e_dependency_solve_failure,
                   e_dependency_solve_failure_explanation explain,
                   const std::vector<e_nonesuch_package>& missing_pkgs)
        ->int {
        bpt_log(
            error,
            "No dependency solution is possible with the known package information: \n{}"_styled,
            explain.value);
        for (auto& missing : missing_pkgs) {
            missing.log_error(
                "Direct requirement on '.bold.red[{}]' does not name an existing package in any enabled repositories"_styled);
        }
        write_error_marker("no-dependency-solution");
        return 1;
    }
    bpt_leaf_catch(user_error<errc::compile_failure>)->int {
        write_error_marker("compile-failed");
        throw;
    }
    bpt_leaf_catch(user_error<errc::link_failure>)->int {
        write_error_marker("link-failed");
        throw;
    }
    bpt_leaf_catch(user_error<errc::test_failure> exc) {
        write_error_marker("build-failed-test-failed");
        bpt_log(error, "{}", exc.what());
        bpt_log(error, "{}", exc.explanation());
        bpt_log(error, "Refer: {}", exc.error_reference());
        return 1;
    };
}
