#include "./remote.hpp"

#include <dds/error/errors.hpp>
#include <dds/http/session.hpp>
#include <dds/temp.hpp>
#include <dds/util/log.hpp>
#include <dds/util/result.hpp>

#include <neo/event.hpp>
#include <neo/scope.hpp>
#include <neo/sqlite3/exec.hpp>
#include <neo/sqlite3/iter_tuples.hpp>
#include <neo/sqlite3/single.hpp>
#include <neo/sqlite3/transaction.hpp>
#include <neo/url.hpp>
#include <neo/utility.hpp>
#include <range/v3/range/conversion.hpp>

using namespace dds;
namespace nsql = neo::sqlite3;

namespace {

struct remote_db {
    temporary_dir  _tempdir;
    nsql::database db;

    static remote_db download_and_open(neo::url const& url) {
        neo_assert(expects,
                   url.host.has_value(),
                   "URL does not have a hostname??",
                   url.to_string());
        auto sess = http_session::connect_for(url);

        auto tempdir    = temporary_dir::create();
        auto repo_db_dl = tempdir.path() / "repo.db";
        fs::create_directories(tempdir.path());
        sess.download_file(
            {
                .method = "GET",
                .path   = url.path,
            },
            repo_db_dl);

        auto db = nsql::open(repo_db_dl.string());
        return {tempdir, std::move(db)};
    }

    static remote_db download_and_open_for_base(neo::url url) {
        auto repo_url = url;
        repo_url.path = fs::path(url.path).append("repo.db").generic_string();
        return download_and_open(repo_url);
    }

    static remote_db download_and_open_for_base(std::string_view url_str) {
        return download_and_open_for_base(neo::url::parse(url_str));
    }
};

}  // namespace

pkg_remote pkg_remote::connect(std::string_view url_str) {
    DDS_E_SCOPE(e_url_string{std::string(url_str)});
    const auto url = neo::url::parse(url_str);

    auto db      = remote_db::download_and_open_for_base(url);
    auto name_st = db.db.prepare("SELECT name FROM dds_repo_meta");
    auto [name]  = nsql::unpack_single<std::string>(name_st);

    return {name, url};
}

void pkg_remote::store(nsql::database_ref db) {
    auto st = db.prepare(R"(
        INSERT INTO dds_cat_remotes (name, gen_ident, remote_url)
            VALUES (?, ?, ?)
        ON CONFLICT (name) DO
            UPDATE SET gen_ident = ?2, remote_url = ?3
    )");
    nsql::exec(st, _name, "[placeholder]", _base_url.to_string());
}

void pkg_remote::update_pkg_db(nsql::database_ref db) {
    dds_log(info, "Pulling repository contents for {} [{}]", _name, _base_url.to_string());

    auto rdb = remote_db::download_and_open_for_base(_base_url);

    auto base_url_str = _base_url.to_string();
    while (base_url_str.ends_with("/")) {
        base_url_str.pop_back();
    }

    auto db_path = rdb._tempdir.path() / "repo.db";

    auto rid_st          = db.prepare("SELECT remote_id FROM dds_cat_remotes WHERE name = ?");
    rid_st.bindings()[1] = _name;
    auto [remote_id]     = nsql::unpack_single<std::int64_t>(rid_st);
    rid_st.reset();

    nsql::exec(db.prepare("ATTACH DATABASE ? AS remote"), db_path.string());
    neo_defer { db.exec("DETACH DATABASE remote"); };
    nsql::transaction_guard tr{db};
    nsql::exec(  //
        db.prepare(R"(
            DELETE FROM dds_cat_pkgs
            WHERE remote_id = ?
        )"),
        remote_id);
    nsql::exec(  //
        db.prepare(R"(
            INSERT INTO dds_cat_pkgs
                (name, version, description, remote_url, remote_id)
            SELECT
                name,
                version,
                description,
                CASE
                    WHEN url LIKE 'dds:%@%' THEN
                        -- Convert 'dds:name@ver' to 'dds+<base-repo-url>/name@ver'
                        -- This will later resolve to the actual package URL
                        printf('dds+%s/%s', ?2, substr(url, 5))
                    ELSE
                        -- Non-'dds:' URLs are kept as-is
                        url
                END,
                ?1
            FROM remote.dds_repo_packages
        )"),
        remote_id,
        base_url_str);
    db.exec(R"(
        INSERT OR REPLACE INTO dds_cat_pkg_deps (pkg_id, dep_name, low, high)
            SELECT
                local_pkgs.pkg_id AS pkg_id,
                dep_name,
                low,
                high
            FROM remote.dds_repo_package_deps AS deps,
                 remote.dds_repo_packages AS pkgs USING(package_id),
                 dds_cat_pkgs AS local_pkgs USING(name, version)
    )");
    // Validate our database
    auto fk_check = db.prepare("PRAGMA foreign_key_check");
    auto rows = nsql::iter_tuples<std::string, std::int64_t, std::string, std::string>(fk_check);
    bool any_failed = false;
    for (auto [child_table, rowid, parent_table, failed_idx] : rows) {
        dds_log(
            critical,
            "Database foreign_key error after import: {0}.{3} referencing {2} violated at row {1}",
            child_table,
            rowid,
            parent_table,
            failed_idx);
        any_failed = true;
    }
    auto int_check = db.prepare("PRAGMA main.integrity_check");
    for (auto [error] : nsql::iter_tuples<std::string>(int_check)) {
        if (error == "ok") {
            continue;
        }
        dds_log(critical, "Database errors after import: {}", error);
        any_failed = true;
    }
    if (any_failed) {
        throw_external_error<errc::corrupted_catalog_db>(
            "Database update failed due to data integrity errors");
    }
}

void dds::update_all_remotes(nsql::database_ref db) {
    dds_log(info, "Updating catalog from all remotes");
    auto repos_st = db.prepare("SELECT name, remote_url FROM dds_cat_remotes");
    auto tups     = nsql::iter_tuples<std::string, std::string>(repos_st) | ranges::to_vector;

    for (const auto& [name, remote_url] : tups) {
        DDS_E_SCOPE(e_url_string{remote_url});
        pkg_remote repo{name, neo::url::parse(remote_url)};
        repo.update_pkg_db(db);
    }

    dds_log(info, "Recompacting database...");
    db.exec("VACUUM");
}
