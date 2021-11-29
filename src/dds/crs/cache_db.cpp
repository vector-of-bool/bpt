#include "./cache_db.hpp"

#include "./error.hpp"

#include <dds/error/handle.hpp>
#include <dds/error/on_error.hpp>
#include <dds/error/result.hpp>
#include <dds/error/try_catch.hpp>
#include <dds/temp.hpp>
#include <dds/util/compress.hpp>
#include <dds/util/db/migrate.hpp>
#include <dds/util/db/query.hpp>
#include <dds/util/fs/path.hpp>
#include <dds/util/http/pool.hpp>
#include <dds/util/http/response.hpp>
#include <dds/util/log.hpp>

#include <boost/leaf.hpp>
#include <fansi/styled.hpp>
#include <neo/assert.hpp>
#include <neo/opt_ref.hpp>
#include <neo/ranges.hpp>
#include <neo/scope.hpp>
#include <neo/sqlite3/connection.hpp>
#include <neo/sqlite3/exec.hpp>
#include <neo/sqlite3/transaction.hpp>
#include <neo/tl.hpp>

using namespace dds;
using namespace dds::crs;
using namespace neo::sqlite3::literals;
using namespace fansi::literals;
using std::optional;
using std::string;
using std::string_view;

neo::sqlite3::statement& cache_db::_prepare(neo::sqlite3::sql_string_literal sql) const {
    return _db.get().prepare(sql);
}

cache_db cache_db::open(unique_database& db) {
    dds::apply_db_migrations(db, "dds_crs_meta", [](auto& db) {
        db.exec_script(R"(
            CREATE TABLE dds_crs_remotes (
                remote_id INTEGER PRIMARY KEY,
                url TEXT NOT NULL,
                unique_name TEXT NOT NULL UNIQUE,
                revno INTEGER NOT NULL,
                etag TEXT,
                last_modified TEXT
            );

            CREATE TABLE dds_crs_packages (
                pkg_id INTEGER PRIMARY KEY,
                json TEXT NOT NULL,
                remote_id INTEGER NOT NULL
                    REFERENCES dds_crs_remotes
                    ON DELETE CASCADE,
                name TEXT NOT NULL
                    GENERATED ALWAYS
                    AS (json_extract(json, '$.name'))
                    STORED,
                namespace TEXT NOT NULL
                    GENERATED ALWAYS
                    AS (json_extract(json, '$.namespace'))
                    STORED,
                version TEXT NOT NULL
                    GENERATED ALWAYS
                    AS (json_extract(json, '$.version'))
                    STORED,
                meta_version INTEGER NOT NULL
                    GENERATED ALWAYS
                    AS (json_extract(json, '$.meta_version'))
                    STORED,
                remote_revno INTEGER NOT NULL,
                UNIQUE (name, version, meta_version, remote_id)
            );

            CREATE TABLE dds_crs_libraries (
                lib_id INTEGER PRIMARY KEY,
                pkg_id INTEGER NOT NULL
                    REFERENCES dds_crs_packages
                    ON DELETE CASCADE,
                name TEXT NOT NULL,
                qualname TEXT NOT NULL,
                path TEXT NOT NULL,
                UNIQUE (pkg_id, name)
            );

            CREATE TRIGGER dds_crs_libraries_auto_insert
                AFTER INSERT ON dds_crs_packages
                FOR EACH ROW
                BEGIN
                    INSERT INTO dds_crs_libraries (pkg_id, name, qualname, path)
                        WITH libraries AS (
                            SELECT value FROM json_each(new.json, '$.libraries')
                        )
                        SELECT
                            new.pkg_id,
                            json_extract(lib.value, '$.name'),
                            printf('%s/%s', new.namespace, json_extract(lib.value, '$.name')),
                            json_extract(lib.value, '$.path')
                        FROM libraries AS lib;
                END;

            CREATE TRIGGER dds_crs_libraries_auto_update
                AFTER UPDATE ON dds_crs_packages
                FOR EACH ROW WHEN new.json != old.json
                BEGIN
                    DELETE FROM dds_crs_libraries
                        WHERE pkg_id = new.pkg_id;
                    INSERT INTO dds_crs_libraries (pkg_id, name, qualname, path)
                        WITH libraries AS (
                            SELECT value FROM json_each(new.json, '$.libraries')
                        )
                        SELECT
                            new.pkg_id,
                            json_extract(lib.value, '$.name'),
                            printf('%s/%s', new.namespace, json_extract(lib.value, '$.name')),
                            json_extract(lib.value, '$.path')
                        FROM libraries AS lib;
                END;
        )"_sql);
    }).value();
    return cache_db{db};
}

static cache_db::package_entry pkg_entry_from_row(auto&& row) noexcept {
    auto&& [rowid, remote_id, json_str] = row;
    auto meta                           = package_meta::from_json_str(json_str);
    return cache_db::package_entry{.package_id = rowid,
                                   .remote_id  = remote_id,
                                   .pkg        = std::move(meta)};
}

void cache_db::forget_all() { db_exec(_prepare("DELETE FROM dds_crs_remotes"_sql)).value(); }

namespace {

std::vector<cache_db::package_entry> cache_entries_for_query(neo::sqlite3::statement& st) {
    auto rst = st.auto_reset();
    return dds_leaf_try->result<std::vector<cache_db::package_entry>> {
        return neo::sqlite3::iter_tuples<std::int64_t,
                                         std::int64_t,
                                         string_view>(st)
            | std::views::transform(NEO_TL(pkg_entry_from_row(_1)))  //
            | neo::to_vector;
    }
    dds_leaf_catch(catch_<neo::sqlite3::error> err)->noreturn_t {
        BOOST_LEAF_THROW_EXCEPTION(err, err.matched.code());
    }
    dds_leaf_catch_all->noreturn_t {
        neo_assert(invariant,
                   false,
                   "Unexpected exception while loading from CRS cache database",
                   diagnostic_info);
    };
}
}  // namespace

std::vector<cache_db::package_entry> cache_db::for_package(dds::name const&       name,
                                                           semver::version const& version) const {
    neo_assertion_breadcrumbs("Loading package cache entries for name+version",
                              name.str,
                              version.to_string());
    auto& st = _prepare(R"(
        SELECT pkg_id, remote_id, json
        FROM dds_crs_packages
        WHERE name = ? AND version = ?
    )"_sql);
    db_bind(st, string_view(name.str), version.to_string()).value();
    return cache_entries_for_query(st);
}

std::vector<cache_db::package_entry> cache_db::for_package(dds::name const& name) const {
    neo_assertion_breadcrumbs("Loading package cache entries for name", name.str);
    auto& st = _prepare(R"(
        SELECT pkg_id, remote_id, json
        FROM dds_crs_packages
        WHERE name = ?
    )"_sql);
    db_bind(st, string_view(name.str)).value();
    return cache_entries_for_query(st);
}

optional<cache_db::remote_entry> cache_db::get_remote(neo::url_view const& url_) const {
    return dds_leaf_try->optional<cache_db::remote_entry> {
        auto url = url_.normalized();
        auto row = db_single<std::int64_t, string, string>(  //
                       _prepare(R"(
                            SELECT remote_id, url, unique_name
                            FROM dds_crs_remotes
                            WHERE url = ?
                       )"_sql),
                       string_view(url_.to_string()))
                       .value();
        auto [rowid, url_str, name] = row;
        return cache_db::remote_entry{rowid, neo::url::parse(url_str), name};
    }
    dds_leaf_catch(matchv<neo::sqlite3::errc::done>) { return std::nullopt; };
}

void cache_db::sync_repo(const neo::url_view& url_) const {
    auto url = url_.normalized();
    DDS_E_SCOPE(e_sync_repo{url});
    dds::http_request_params params;

    auto& prio_info_st
        = _prepare("SELECT etag, last_modified FROM dds_crs_remotes WHERE url=?"_sql);
    auto url_str = url.to_string();
    neo_assertion_breadcrumbs("Pulling repository metadata", url_str);
    neo::sqlite3::reset_and_bind(prio_info_st, std::string_view(url_str)).throw_if_error();
    auto prior_info = neo::sqlite3::one_row<std::optional<std::string>, std::optional<std::string>>(
        prio_info_st);
    if (prior_info.has_value()) {
        dds_log(debug, "Seen this repo before. Chacking for updates.");
        const auto& [etag, last_mod] = *prior_info;
        if (etag.has_value()) {
            params.prior_etag = *etag;
        }
        if (last_mod.has_value()) {
            params.last_modified = *last_mod;
        }
    }
    if (!prior_info.has_value() && prior_info.errc() != neo::sqlite3::errc::done) {
        prior_info.throw_error();
    }

    auto& pool = dds::http_pool::thread_local_pool();

    auto repo_db_gz_url = url / "repo.db.gz";

    bool           do_discard = true;
    request_result rinfo      = pool.request(repo_db_gz_url, params);
    neo_defer {
        if (do_discard) {
            rinfo.client.abort_client();
        }
    };

    if (rinfo.resp.not_modified()) {
        dds_log(info, "Repository data from .cyan[{}] is up-to-date"_styled, url_str);
        do_discard = false;
        rinfo.discard_body();
        return;
    }

    dds_log(info, "Syncing repository .cyan[{}] ..."_styled, url_str);

    // pool.request() will resolve redirects and errors
    neo_assert(invariant,
               !rinfo.resp.is_redirect(),
               "Did not expect an HTTP redirect at this IO layer");
    neo_assert(invariant, !rinfo.resp.is_error(), "Did not expect an HTTP error at this IO layer");

    auto tmpdir    = dds::temporary_dir::create();
    auto dest_file = tmpdir.path() / "repo.db.gz";
    std::filesystem::create_directories(tmpdir.path());
    rinfo.save_file(dest_file);
    do_discard = false;

    auto repo_db = tmpdir.path() / "repo.db";
    dds::decompress_file_gz(dest_file, repo_db).value();

    neo::sqlite3::exec(_prepare("ATTACH DATABASE ? AS remote"_sql), repo_db.string())
        .throw_if_error();
    neo_defer { _db.get().exec_script(R"(DETACH DATABASE remote)"_sql); };

    // Import those packages
    neo::sqlite3::transaction_guard tr{_db.get().raw_database()};

    auto& update_remote_st = _prepare(R"(
        INSERT INTO dds_crs_remotes
            (url, unique_name, revno, etag, last_modified)
        VALUES (
            :url,  -- url
            (SELECT name FROM remote.crs_repo_self), -- unique_name
            1,  -- revno
            :etag,  -- etag
            :last_modified  -- last_modified
        )
        ON CONFLICT (unique_name) DO UPDATE
            SET url = :url,
                etag = :etag,
                last_modified = :last_modified,
                revno = revno + 1
        RETURNING remote_id, revno
    )"_sql);
    update_remote_st.reset();
    update_remote_st.bindings().clear();
    update_remote_st.bindings()[":url"] = std::string_view(url_str);
    if (auto etag_ = rinfo.resp.etag()) {
        update_remote_st.bindings()[":etag"] = *etag_;
    }
    if (auto last_mod_ = rinfo.resp.last_modified()) {
        update_remote_st.bindings()[":last_modified"] = *last_mod_;
    }
    const auto [remote_id, remote_revno]
        = *neo::sqlite3::one_row<std::int64_t, std::int64_t>(update_remote_st);

    auto& update_pkg_st = _prepare(R"(
        INSERT INTO dds_crs_packages (json, remote_id, remote_revno)
            SELECT pkg.meta_json, $remote_id, $remote_revno
              FROM remote.crs_repo_packages AS pkg
              WHERE 1  -- Fix parse ambiguity with upsert
        ON CONFLICT(name, version, meta_version, remote_id) DO UPDATE
            SET json=excluded.json,
                remote_revno=$remote_revno
    )"_sql);
    update_pkg_st.reset();
    update_pkg_st.bindings()["$remote_revno"] = remote_revno;
    update_pkg_st.bindings()["$remote_id"]    = remote_id;
    neo::sqlite3::exec(update_pkg_st).throw_if_error();
    const auto n_updated = _db.get().raw_database().changes();

    auto& delete_old_st = _prepare(R"(
        DELETE FROM dds_crs_packages
            WHERE remote_id = ? AND remote_revno < ?
    )"_sql);
    neo::sqlite3::reset_and_bind(delete_old_st, remote_id, remote_revno).throw_if_error();
    neo::sqlite3::exec(delete_old_st).throw_if_error();
    const auto n_deleted = _db.get().raw_database().changes();

    dds_log(info,
            "Syncing repository .cyan[{}] Done: {} added/updated, {} deleted"_styled,
            url_str,
            n_updated,
            n_deleted);
}
