#include "../options.hpp"

#include <dds/crs/cache_db.hpp>
#include <dds/crs/repo.hpp>
#include <dds/error/marker.hpp>
#include <dds/error/try_catch.hpp>
#include <dds/solve/solve.hpp>
#include <dds/util/signal.hpp>
#include <dds/util/url.hpp>

#include <fansi/styled.hpp>
#include <fmt/ostream.h>
#include <neo/ranges.hpp>
#include <neo/ref.hpp>
#include <neo/scope.hpp>
#include <neo/sqlite3/connection.hpp>
#include <neo/sqlite3/exec.hpp>
#include <neo/sqlite3/transaction.hpp>
#include <neo/tl.hpp>

#include <iostream>
#include <ranges>

using namespace neo::sqlite3::literals;
using namespace fansi::literals;

namespace dds::cli::cmd {

static bool try_it(const crs::package_meta& pkg, crs::cache_db& cache) {
    auto dep = {crs::dependency{
        .name                = pkg.name,
        .acceptable_versions = crs::version_range_set{pkg.version, pkg.version.next_after()},
        .kind                = crs::usage_kind::lib,
        .uses = crs::explicit_uses_list{pkg.libraries | std::views::transform(NEO_TL(_1.name))
                                        | neo::to_vector},
    }};
    return dds_leaf_try {
        fmt::print("Validate package .br.cyan[{}@{}~{}] ..."_styled,
                   pkg.name.str,
                   pkg.version.to_string(),
                   pkg.meta_version);
        std::cout.flush();
        neo_defer { fmt::print("\r\x1b[K"); };
        dds::solve(cache, dep);
        return true;
    }
    dds_leaf_catch(e_usage_no_such_lib,
                   lm::usage bad_usage,
                   crs::dependency,
                   crs::package_meta dep_pkg) {
        dds_log(error,
                "Package .bold.red[{}@{}~{}] is not valid:"_styled,
                pkg.name.str,
                pkg.version.to_string(),
                pkg.meta_version);
        dds_log(
            error,
            "It requests a usage of library .br.red[{}] from .br.yellow[{}@{}], which does not exist in that package."_styled,
            bad_usage,
            dep_pkg.name.str,
            dep_pkg.version.to_string());
        auto yellows
            = dep_pkg.libraries | std::views::transform([&](auto&& _1) {
                  return fmt::format(".yellow[{}/{}]"_styled, dep_pkg.namespace_.str, _1.name.str);
              });
        dds_log(error,
                "  (.br.yellow[{}@{}] defines {})"_styled,
                dep_pkg.name.str,
                dep_pkg.version.to_string(),
                joinstr(", ", yellows));
        return false;
    }
    dds_leaf_catch(e_dependency_solve_failure, e_dependency_solve_failure_explanation explain) {
        dds_log(
            error,
            "Installation of .bold.red[{}@{}-{}] is not possible with the known package information: \n{}"_styled,
            pkg.name.str,
            pkg.version.to_string(),
            pkg.meta_version,
            explain.value);
        return false;
    }
    dds_leaf_catch(dds::user_cancelled)->dds::noreturn_t { throw; }
    dds_leaf_catch_all {
        dds_log(error, "Package validation error: {}", diagnostic_info);
        return false;
    };
}

int repo_validate(const options& opts) {
    auto repo     = dds::crs::repository::open_existing(opts.repo.repo_dir);
    auto db       = dds::unique_database::open("").value();
    auto cache    = dds::crs::cache_db::open(db);
    int  n_errors = 0;

    for (auto& r : opts.use_repos) {
        auto url = dds::guess_url_from_string(r);
        cache.sync_remote(url);
        cache.enable_remote(url);
    }

    neo::sqlite3::transaction_guard tr{db.raw_database()};
    neo_defer { tr.rollback(); };

    auto tmp_remote_id = *neo::sqlite3::one_cell<std::int64_t>(db.prepare(R"(
        INSERT INTO dds_crs_remotes (url, unique_name, revno)
        VALUES ('tmp-validate', '--- tmp-validation-repo ---', 1)
        RETURNING remote_id
    )"_sql));
    neo::sqlite3::exec(db.prepare("INSERT INTO dds_crs_enabled_remotes (remote_id) VALUES(?)"_sql),
                       tmp_remote_id)
        .throw_if_error();

    neo::sqlite3::exec_each(  //
        db.prepare(R"(
            INSERT INTO dds_crs_packages(json, remote_id, remote_revno)
            VALUES (?, ?, 1)
        )"_sql),
        repo.all_packages() | neo::lref
            | std::views::transform(NEO_TL(std::tuple(_1.to_json(), tmp_remote_id))))
        .throw_if_error();

    for (auto&& pkg : repo.all_packages()) {
        dds::cancellation_point();
        const bool okay = try_it(pkg, cache);
        if (!okay) {
            n_errors++;
        }
    }

    if (n_errors) {
        if (n_errors == 1) {
            dds_log(error, ".bold.red[1 package] is not possibly installible"_styled);
        } else {
            dds_log(error, ".bold.red[{} packages] are not possibly installible"_styled, n_errors);
        }
        write_error_marker("repo-invalid");
        return 1;
    }
    return 0;
}

}  // namespace dds::cli::cmd
