#include "./remote.hpp"

#include <dds/error/errors.hpp>
#include <dds/temp.hpp>
#include <dds/util/http/pool.hpp>
#include <dds/util/log.hpp>
#include <dds/util/result.hpp>

#include <neo/event.hpp>
#include <neo/io/stream/buffers.hpp>
#include <neo/io/stream/file.hpp>
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

    static remote_db download_and_open(http_client& client, const http_response_info& resp) {
        auto tempdir    = temporary_dir::create();
        auto repo_db_dl = tempdir.path() / "repo.db";
        fs::create_directories(tempdir.path());
        auto outfile = neo::file_stream::open(repo_db_dl, neo::open_mode::write);
        client.recv_body_into(resp, neo::stream_io_buffers(outfile));

        auto db = nsql::open(repo_db_dl.string());
        return {tempdir, std::move(db)};
    }
};

}  // namespace

pkg_remote pkg_remote::connect(std::string_view url_str) {
    DDS_E_SCOPE(e_url_string{std::string(url_str)});
    const auto url = neo::url::parse(url_str);

    auto& pool   = http_pool::global_pool();
    auto  db_url = url;
    while (db_url.path.ends_with("/"))
        db_url.path.pop_back();
    auto full_path      = fmt::format("{}/{}", db_url.path, "repo.db");
    db_url.path         = full_path;
    auto [client, resp] = pool.request(db_url, http_request_params{.method = "GET"});
    auto db             = remote_db::download_and_open(client, resp);

    auto name_st = db.db.prepare("SELECT name FROM dds_repo_meta");
    auto [name]  = nsql::unpack_single<std::string>(name_st);

    return {name, url};
}

void pkg_remote::store(nsql::database_ref db) {
    auto st = db.prepare(R"(
        INSERT INTO dds_pkg_remotes (name, remote_url)
            VALUES (?, ?)
        ON CONFLICT (name) DO
            UPDATE SET remote_url = ?2
    )");
    nsql::exec(st, _name, _base_url.to_string());
}

void pkg_remote::update_pkg_db(nsql::database_ref              db,
                               std::optional<std::string_view> etag,
                               std::optional<std::string_view> db_mtime) {
    dds_log(info, "Pulling repository contents for {} [{}]", _name, _base_url.to_string());

    auto& pool = http_pool::global_pool();
    auto  url  = _base_url;
    while (url.path.ends_with("/"))
        url.path.pop_back();
    auto full_path      = fmt::format("{}/{}", url.path, "repo.db");
    url.path            = full_path;
    auto [client, resp] = pool.request(url,
                                       http_request_params{
                                           .method        = "GET",
                                           .prior_etag    = etag.value_or(""),
                                           .last_modified = db_mtime.value_or(""),
                                       });
    if (resp.not_modified()) {
        // Cache hit
        dds_log(info, "Package database {} is up-to-date", _name);
        client.discard_body(resp);
        return;
    }

    auto rdb = remote_db::download_and_open(client, resp);

    auto base_url_str = _base_url.to_string();
    while (base_url_str.ends_with("/")) {
        base_url_str.pop_back();
    }

    auto db_path = rdb._tempdir.path() / "repo.db";

    auto rid_st          = db.prepare("SELECT remote_id FROM dds_pkg_remotes WHERE name = ?");
    rid_st.bindings()[1] = _name;
    auto [remote_id]     = nsql::unpack_single<std::int64_t>(rid_st);
    rid_st.reset();

    dds_log(trace, "Attaching downloaded database");
    nsql::exec(db.prepare("ATTACH DATABASE ? AS remote"), db_path.string());
    neo_defer { db.exec("DETACH DATABASE remote"); };
    nsql::transaction_guard tr{db};
    dds_log(trace, "Clearing prior contents");
    nsql::exec(  //
        db.prepare(R"(
            DELETE FROM dds_pkgs
            WHERE remote_id = ?
        )"),
        remote_id);
    dds_log(trace, "Importing packages");
    nsql::exec(  //
        db.prepare(R"(
            INSERT INTO dds_pkgs
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
    dds_log(trace, "Importing dependencies");
    db.exec(R"(
        INSERT OR REPLACE INTO dds_pkg_deps (pkg_id, dep_name, low, high)
            SELECT
                local_pkgs.pkg_id AS pkg_id,
                dep_name,
                low,
                high
            FROM remote.dds_repo_package_deps AS deps,
                 remote.dds_repo_packages AS pkgs USING(package_id),
                 dds_pkgs AS local_pkgs USING(name, version)
    )");
    // Validate our database
    dds_log(trace, "Running integrity check");
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

    // Save the cache info for the remote
    if (auto new_etag = resp.etag()) {
        nsql::exec(db.prepare("UPDATE dds_pkg_remotes SET db_etag = ? WHERE name = ?"),
                   *new_etag,
                   _name);
    }
    if (auto mtime = resp.last_modified()) {
        nsql::exec(db.prepare("UPDATE dds_pkg_remotes SET db_mtime = ? WHERE name = ?"),
                   *mtime,
                   _name);
    }
}

void dds::update_all_remotes(nsql::database_ref db) {
    dds_log(info, "Updating catalog from all remotes");
    auto repos_st = db.prepare("SELECT name, remote_url, db_etag, db_mtime FROM dds_pkg_remotes");
    auto tups     = nsql::iter_tuples<std::string,
                                  std::string,
                                  std::optional<std::string>,
                                  std::optional<std::string>>(repos_st)
        | ranges::to_vector;

    for (const auto& [name, remote_url, etag, db_mtime] : tups) {
        DDS_E_SCOPE(e_url_string{remote_url});
        pkg_remote repo{name, neo::url::parse(remote_url)};
        repo.update_pkg_db(db, etag, db_mtime);
    }

    dds_log(info, "Recompacting database...");
    db.exec("VACUUM");
}
