#include "./build_common.hpp"

#include <dds/crs/cache.hpp>
#include <dds/crs/cache_db.hpp>
#include <dds/crs/error.hpp>
#include <dds/crs/info/dependency.hpp>
#include <dds/error/errors.hpp>
#include <dds/error/marker.hpp>
#include <dds/error/on_error.hpp>
#include <dds/error/try_catch.hpp>
#include <dds/project/project.hpp>
#include <dds/sdist/error.hpp>
#include <dds/solve/solve.hpp>
#include <dds/util/algo.hpp>
#include <dds/util/fs/io.hpp>
#include <dds/util/http/error.hpp>
#include <dds/util/http/pool.hpp>
#include <dds/util/url.hpp>

#include <boost/leaf/pred.hpp>
#include <fansi/styled.hpp>
#include <neo/ranges.hpp>
#include <neo/sqlite3/error.hpp>
#include <neo/tl.hpp>

using namespace dds;
using namespace fansi::literals;

crs::cache cli::open_ready_cache(const cli::options& opts) {
    auto cache
        = dds::crs::cache::open(opts.crs_cache_dir.value_or(dds::crs::cache::default_path()));
    auto& meta_db = cache.metadata_db();
    for (auto& r : opts.use_repos) {
        // Convert what may be just a domain name or partial URL into a proper URL:
        auto url = dds::guess_url_from_string(r);
        // Called by error handler to decide whether to rethrow:
        auto check_cache_after_error = [&](auto&&... rethrow) {
            if (opts.repo_sync_mode == cli::repo_sync_mode::always) {
                // We should always sync package listings, so this is a hard error
                BOOST_LEAF_THROW_EXCEPTION(NEO_FWD(rethrow)...);
            }
            auto rid = meta_db.get_remote(url);
            if (rid.has_value()) {
                // We have prior metadata for the given repository.
                dds_log(warn,
                        "We'll continue by using cached information for .bold.yellow[{}]"_styled,
                        url.to_string());
            } else {
                dds_log(
                    error,
                    "We have no cached metadata for .bold.red[{}], and were unable to obtain any."_styled,
                    url.to_string());
                BOOST_LEAF_THROW_EXCEPTION(NEO_FWD(rethrow)...);
            }
        };
        dds_leaf_try {
            using m = cli::repo_sync_mode;
            switch (opts.repo_sync_mode) {
            case m::cached_okay:
            case m::always:
                meta_db.sync_remote(url);
                return;
            case m::never:
                return;
            }
        }
        dds_leaf_catch(const std::system_error& e, neo::url e_url, http_response_info resp) {
            dds_log(error,
                    "An error occurred while downloading [.bold.red[{}]]: {}"_styled,
                    e_url.to_string(),
                    e.code().message());
            check_cache_after_error(e, e_url, resp);
        }
        dds_leaf_catch(const std::system_error& e, network_origin origin, neo::url const* e_url) {
            dds_log(error,
                    "Network error communicating with .bold.red[{}://{}:{}]: {}"_styled,
                    origin.protocol,
                    origin.hostname,
                    origin.port,
                    e.code().message());
            if (e_url) {
                dds_log(error,
                        "  (While accessing URL [.bold.red[{}]])"_styled,
                        e_url->to_string());
                check_cache_after_error(e, origin, *e_url);
            } else {
                check_cache_after_error(e, origin);
            }
        };
        meta_db.enable_remote(url);
    }
    return cache;
}

static void resolve_implicit_usages(crs::package_info& proj_meta, crs::package_info& dep_meta) {
    proj_meta.libraries                                                         //
        | std::views::transform(NEO_TL(_1.dependencies))                        //
        | std::views::join                                                      //
        | std::views::filter(NEO_TL(_1.name == dep_meta.name))                  //
        | std::views::transform(NEO_TL(_1.uses))                                //
        | std::views::filter(NEO_TL(_1.template is<crs::implicit_uses_all>()))  //
        | neo::ranges::each([&](crs::dependency_uses& item) {
              item = crs::explicit_uses_list{dep_meta.libraries
                                             | std::views::transform(&dds::crs::library_info::name)
                                             | neo::to_vector};
          });
}

builder dds::cli::create_project_builder(const dds::cli::options& opts) {
    sdist_build_params main_params = {
        .subdir          = "",
        .build_tests     = opts.build.want_tests,
        .run_tests       = opts.build.want_tests,
        .build_apps      = opts.build.want_apps,
        .enable_warnings = !opts.disable_warnings,
    };

    auto  cache   = open_ready_cache(opts);
    auto& meta_db = cache.metadata_db();

    sdist proj_sd = dds_leaf_try { return sdist::from_directory(opts.project_dir); }
    dds_leaf_catch(dds::e_missing_pkg_json, dds::e_missing_project_yaml) {
        crs::package_info default_meta;
        default_meta.name.str     = "anon";
        default_meta.pkg_revision = 0;
        crs::library_info default_library;
        default_library.name.str = "anon";
        default_meta.libraries.push_back(default_library);
        return sdist{std::move(default_meta), opts.project_dir};
    };

    builder builder;
    if (!opts.build.lm_index.has_value()) {
        auto crs_deps = proj_sd.pkg.libraries | std::views::transform(NEO_TL(_1.dependencies))
            | std::views::join;

        auto sln = dds::solve(meta_db, crs_deps);
        for (auto&& pkg : sln) {
            auto dep_meta = fetch_cache_load_dependency(cache, pkg, builder, "_deps");
            resolve_implicit_usages(proj_sd.pkg, dep_meta);
        }
    }
    builder.add(proj_sd, main_params);
    return builder;
}

crs::package_info dds::cli::fetch_cache_load_dependency(crs::cache&        cache,
                                                        crs::pkg_id const& pkg,
                                                        dds::builder&      builder,
                                                        path_ref           subdir_base) {
    dds_log(debug, "Loading package '{}' for build", pkg.to_string());
    auto local_dir        = cache.prefetch(pkg);
    auto pkg_json_path    = local_dir / "pkg.json";
    auto pkg_json_content = dds::read_file(pkg_json_path);
    DDS_E_SCOPE(crs::e_pkg_json_path{pkg_json_path});
    auto crs_meta = crs::package_info::from_json_str(pkg_json_content);

    dds::sdist         sd{crs_meta, local_dir};
    sdist_build_params params;
    params.subdir = subdir_base / sd.pkg.id().to_string();
    builder.add(sd, params);
    return crs_meta;
}

int dds::cli::handle_build_error(std::function<int()> fn) {
    return dds_leaf_try { return fn(); }
    dds_leaf_catch(e_dependency_solve_failure, e_dependency_solve_failure_explanation explain)
        ->int {
        dds_log(
            error,
            "No dependency solution is possible with the known package information: \n{}"_styled,
            explain.value);
        write_error_marker("no-dependency-solution");
        return 1;
    }
    dds_leaf_catch(user_error<errc::compile_failure>)->int {
        write_error_marker("compile-failed");
        throw;
    }
    dds_leaf_catch(user_error<errc::test_failure> exc) {
        write_error_marker("build-failed-test-failed");
        dds_log(error, "{}", exc.what());
        dds_log(error, "{}", exc.explanation());
        dds_log(error, "Refer: {}", exc.error_reference());
        return 1;
    };
}
