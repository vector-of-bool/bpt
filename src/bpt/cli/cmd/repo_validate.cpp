#include "../options.hpp"

#include <bpt/crs/cache_db.hpp>
#include <bpt/crs/repo.hpp>
#include <bpt/error/marker.hpp>
#include <bpt/error/try_catch.hpp>
#include <bpt/solve/solve.hpp>
#include <bpt/util/signal.hpp>
#include <bpt/util/url.hpp>

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

namespace bpt::cli::cmd {

static bool try_it(const crs::package_info& pkg, crs::cache_db& cache) {
    auto dep = {crs::dependency{
        .name                = pkg.id.name,
        .acceptable_versions = crs::version_range_set{pkg.id.version, pkg.id.version.next_after()},
        .uses = crs::explicit_uses_list{pkg.libraries | std::views::transform(NEO_TL(_1.name))
                                        | neo::to_vector},
    }};
    return bpt_leaf_try {
        fmt::print("Validate package .br.cyan[{}] ..."_styled, pkg.id.to_string());
        std::cout.flush();
        neo_defer { fmt::print("\r\x1b[K"); };
        bpt::solve(cache, dep);
        return true;
    }
    bpt_leaf_catch(e_usage_no_such_lib,
                   lm::usage bad_usage,
                   crs::dependency,
                   crs::package_info dep_pkg) {
        bpt_log(error, "Package .bold.red[{}] is not valid:"_styled, pkg.id.to_string());
        bpt_log(
            error,
            "It requests a usage of library .br.red[{}] from .br.yellow[{}], which does not exist in that package."_styled,
            bad_usage,
            dep_pkg.id.to_string());
        auto yellows
            = dep_pkg.libraries | std::views::transform([&](auto&& _1) {
                  return fmt::format(".yellow[{}/{}]"_styled, dep_pkg.id.name.str, _1.name.str);
              });
        bpt_log(error,
                "  (.br.yellow[{}] defines {})"_styled,
                dep_pkg.id.to_string(),
                joinstr(", ", yellows));
        return false;
    }
    bpt_leaf_catch(e_dependency_solve_failure, e_dependency_solve_failure_explanation explain) {
        bpt_log(
            error,
            "Installation of .bold.red[{}] is not possible with the known package information: \n{}"_styled,
            pkg.id.to_string(),
            explain.value);
        return false;
    }
    bpt_leaf_catch(bpt::user_cancelled)->bpt::noreturn_t { throw; }
    bpt_leaf_catch_all {
        bpt_log(error, "Package validation error: {}", diagnostic_info);
        return false;
    };
}

int repo_validate(const options& opts) {
    auto repo     = bpt::crs::repository::open_existing(opts.repo.repo_dir);
    auto db       = bpt::unique_database::open("").value();
    auto cache    = bpt::crs::cache_db::open(db);
    int  n_errors = 0;

    for (auto& r : opts.use_repos) {
        auto url = bpt::guess_url_from_string(r);
        cache.sync_remote(url);
        cache.enable_remote(url);
    }

    neo::sqlite3::transaction_guard tr{db.sqlite3_db()};
    neo_defer { tr.rollback(); };

    auto tmp_remote_id = *neo::sqlite3::one_cell<std::int64_t>(db.prepare(R"(
        INSERT INTO bpt_crs_remotes (url, unique_name, revno)
        VALUES ('tmp-validate', '--- tmp-validation-repo ---', 1)
        RETURNING remote_id
    )"_sql));
    neo::sqlite3::exec(db.prepare("INSERT INTO bpt_crs_enabled_remotes (remote_id) VALUES(?)"_sql),
                       tmp_remote_id)
        .throw_if_error();

    neo::sqlite3::exec_each(  //
        db.prepare(R"(
            INSERT INTO bpt_crs_packages(json, remote_id, remote_revno)
            VALUES (?, ?, 1)
        )"_sql),
        repo.all_packages() | neo::lref | std::views::transform([&](const auto& pkg) {
            return std::make_tuple(pkg.to_json(), tmp_remote_id);
        }))
        .throw_if_error();

    for (auto&& pkg : repo.all_packages()) {
        bpt::cancellation_point();
        const bool okay = try_it(pkg, cache);
        if (!okay) {
            n_errors++;
        }
    }

    if (n_errors) {
        if (n_errors == 1) {
            bpt_log(error, ".bold.red[1 package] is not possibly installible"_styled);
        } else {
            bpt_log(error, ".bold.red[{} packages] are not possibly installible"_styled, n_errors);
        }
        write_error_marker("repo-invalid");
        return 1;
    }
    return 0;
}

}  // namespace bpt::cli::cmd
