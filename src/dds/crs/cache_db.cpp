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
#include <dds/util/url.hpp>

#include <boost/leaf.hpp>
#include <fansi/styled.hpp>
#include <neo/any_range.hpp>
#include <neo/assert.hpp>
#include <neo/co_resource.hpp>
#include <neo/memory.hpp>
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
                remote_revno INTEGER NOT NULL,
                name TEXT NOT NULL
                    GENERATED ALWAYS
                    AS (json_extract(json, '$.name'))
                    STORED,
                version TEXT NOT NULL
                    GENERATED ALWAYS
                    AS (json_extract(json, '$.version'))
                    STORED,
                pkg_revision INTEGER NOT NULL
                    GENERATED ALWAYS
                    AS (json_extract(json, '$.pkg_revision'))
                    STORED,
                UNIQUE (name, version, pkg_revision, remote_id)
            );

            CREATE TABLE dds_crs_libraries (
                lib_id INTEGER PRIMARY KEY,
                pkg_id INTEGER NOT NULL
                    REFERENCES dds_crs_packages
                    ON DELETE CASCADE,
                name TEXT NOT NULL,
                path TEXT NOT NULL,
                UNIQUE (pkg_id, name)
            );

            CREATE TRIGGER dds_crs_libraries_auto_insert
                AFTER INSERT ON dds_crs_packages
                FOR EACH ROW
                BEGIN
                    INSERT INTO dds_crs_libraries (pkg_id, name, path)
                        WITH libraries AS (
                            SELECT value FROM json_each(new.json, '$.libraries')
                        )
                        SELECT
                            new.pkg_id,
                            json_extract(lib.value, '$.name'),
                            json_extract(lib.value, '$.path')
                        FROM libraries AS lib;
                END;

            CREATE TRIGGER dds_crs_libraries_auto_update
                AFTER UPDATE ON dds_crs_packages
                FOR EACH ROW WHEN new.json != old.json
                BEGIN
                    DELETE FROM dds_crs_libraries
                        WHERE pkg_id = new.pkg_id;
                    INSERT INTO dds_crs_libraries (pkg_id, name, path)
                        WITH libraries AS (
                            SELECT value FROM json_each(new.json, '$.libraries')
                        )
                        SELECT
                            new.pkg_id,
                            json_extract(lib.value, '$.name'),
                            json_extract(lib.value, '$.path')
                        FROM libraries AS lib;
                END;
        )"_sql);
    }).value();
    db.exec_script(R"(
        CREATE TEMPORARY TABLE IF NOT EXISTS dds_crs_enabled_remotes (
            remote_id INTEGER NOT NULL -- references main.dds_crs_remotes
                UNIQUE ON CONFLICT IGNORE
        );
    )"_sql);
    return cache_db{db};
}

static cache_db::package_entry pkg_entry_from_row(auto&& row) noexcept {
    auto&& [rowid, remote_id, json_str] = row;
    auto meta                           = package_info::from_json_str(json_str);
    return cache_db::package_entry{.package_id = rowid,
                                   .remote_id  = remote_id,
                                   .pkg        = std::move(meta)};
}

void cache_db::forget_all() { db_exec(_prepare("DELETE FROM dds_crs_remotes"_sql)).value(); }

namespace {

neo::any_input_range<cache_db::package_entry>
cache_entries_for_query(neo::sqlite3::statement&& st) {
    return dds_leaf_try->result<neo::any_input_range<cache_db::package_entry>> {
        auto pin = neo::copy_shared(std::move(st));
        return neo::sqlite3::iter_tuples<std::int64_t, std::int64_t, string_view>(*pin)
            | std::views::transform([pin](auto row) { return pkg_entry_from_row(row); })  //
            ;
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

neo::any_input_range<cache_db::package_entry>
cache_db::for_package(dds::name const& name, semver::version const& version) const {
    neo_assertion_breadcrumbs("Loading package cache entries for name+version",
                              name.str,
                              version.to_string());
    auto st       = *_db.get().raw_database().prepare(R"(
        SELECT pkg_id, remote_id, json
        FROM dds_crs_packages
        WHERE name = ? AND version = ?
            AND remote_id IN (SELECT remote_id FROM dds_crs_enabled_remotes)
    )");
    st.bindings() = std::forward_as_tuple(name.str, version.to_string());
    return cache_entries_for_query(std::move(st));
}

neo::any_input_range<cache_db::package_entry> cache_db::for_package(dds::name const& name) const {
    neo_assertion_breadcrumbs("Loading package cache entries for name", name.str);
    auto st       = *_db.get().raw_database().prepare(R"(
        SELECT pkg_id, remote_id, json
        FROM dds_crs_packages
        WHERE name = ?
            AND remote_id IN (SELECT remote_id FROM dds_crs_enabled_remotes)
    )");
    st.bindings() = std::forward_as_tuple(name.str);
    db_bind(st, string_view(name.str)).value();
    return cache_entries_for_query(std::move(st));
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
                       string_view(url.to_string()))
                       .value();
        auto [rowid, url_str, name] = row;
        return cache_db::remote_entry{rowid, dds::parse_url(url_str), name};
    }
    dds_leaf_catch(matchv<neo::sqlite3::errc::done>) { return std::nullopt; };
}

optional<cache_db::remote_entry> cache_db::get_remote_by_id(std::int64_t rowid) const {
    auto row = neo::sqlite3::one_row<string, string>(  //
        _prepare(R"(
            SELECT url, unique_name
            FROM dds_crs_remotes WHERE remote_id = ?
        )"_sql),
        rowid);
    if (row.has_value()) {
        auto [url_str, name] = *row;
        return remote_entry{rowid, dds::parse_url(url_str), name};
    }
    return std::nullopt;
}

void cache_db::enable_remote(neo::url_view const& url_) {
    auto url = url_.normalized();
    auto res = neo::sqlite3::one_row(  //
        _prepare(R"(
            INSERT INTO dds_crs_enabled_remotes (remote_id)
            SELECT remote_id
              FROM dds_crs_remotes
             WHERE url = ?
            RETURNING remote_id
        )"_sql),
        string_view(url.to_string()));
    if (res == neo::sqlite3::errc::done) {
        BOOST_LEAF_THROW_EXCEPTION(e_no_such_remote_url{url.to_string()});
    }
}

namespace {

struct remote_repo_db_info {
    fs::path                        local_path;
    std::optional<std::string_view> etag;
    std::optional<std::string_view> last_modified;
    bool                            up_to_date;
};

neo::co_resource<remote_repo_db_info> get_remote_db(dds::unique_database& db, neo::url url) {
    if (url.scheme == "file") {
        auto path = fs::path(url.path);
        dds_log(info, "Importing local repository .cyan[{}] ..."_styled, url.path);
        auto info = remote_repo_db_info{
            .local_path    = path / "repo.db",
            .etag          = std::nullopt,
            .last_modified = std::nullopt,
            .up_to_date    = false,
        };
        co_yield info;
    } else {
        dds::http_request_params params;
        auto&                    prio_info_st
            = db.prepare("SELECT etag, last_modified FROM dds_crs_remotes WHERE url=?"_sql);
        auto url_str = url.to_string();
        neo_assertion_breadcrumbs("Pulling remote repository metadata", url_str);
        neo::sqlite3::reset_and_bind(prio_info_st, std::string_view(url_str)).throw_if_error();
        auto prior_info
            = neo::sqlite3::one_row<std::optional<std::string>, std::optional<std::string>>(
                prio_info_st);
        if (prior_info.has_value()) {
            dds_log(debug, "Seen this remote repository before. Checking for updates.");
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
            auto info = remote_repo_db_info{
                .local_path    = "",
                .etag          = rinfo.resp.etag(),
                .last_modified = rinfo.resp.last_modified(),
                .up_to_date    = true,
            };
            co_yield info;
            co_return;
        }

        dds_log(info, "Syncing repository .cyan[{}] ..."_styled, url_str);

        // pool.request() will resolve redirects and errors
        neo_assert(invariant,
                   !rinfo.resp.is_redirect(),
                   "Did not expect an HTTP redirect at this IO layer");
        neo_assert(invariant,
                   !rinfo.resp.is_error(),
                   "Did not expect an HTTP error at this IO layer");

        auto tmpdir    = dds::temporary_dir::create();
        auto dest_file = tmpdir.path() / "repo.db.gz";
        std::filesystem::create_directories(tmpdir.path());
        rinfo.save_file(dest_file);
        do_discard = false;

        auto repo_db = tmpdir.path() / "repo.db";
        dds::decompress_file_gz(dest_file, repo_db).value();
        auto info = remote_repo_db_info{
            .local_path    = repo_db,
            .etag          = rinfo.resp.etag(),
            .last_modified = rinfo.resp.last_modified(),
            .up_to_date    = false,
        };
        co_yield info;
    }
}

}  // namespace

void cache_db::sync_remote(const neo::url_view& url_) const {
    dds::unique_database& db  = _db;
    auto                  url = url_.normalized();
    DDS_E_SCOPE(e_sync_remote{url});
    auto remote_db = get_remote_db(_db, url);

    if (remote_db->up_to_date) {
        return;
    }

    neo::sqlite3::exec(db.prepare("ATTACH DATABASE ? AS remote"_sql),
                       remote_db->local_path.string())
        .throw_if_error();
    neo_defer { db.exec_script(R"(DETACH DATABASE remote)"_sql); };

    // Import those packages
    neo::sqlite3::transaction_guard tr{db.raw_database()};

    auto& update_remote_st = db.prepare(R"(
        INSERT INTO dds_crs_remotes
            (url, unique_name, revno, etag, last_modified)
        VALUES (
            ?1,  -- url
            (SELECT name FROM remote.crs_repo_self), -- unique_name
            1,  -- revno
            ?2,  -- etag
            ?3  -- last_modified
        )
        ON CONFLICT (unique_name) DO UPDATE
            SET url = ?1,
                etag = ?2,
                last_modified = ?3,
                revno = revno + 1
        RETURNING remote_id, revno
    )"_sql);
    auto [remote_id, remote_revno]
        = *neo::sqlite3::one_row<std::int64_t, std::int64_t>(update_remote_st,
                                                             url.to_string(),
                                                             remote_db->etag,
                                                             remote_db->last_modified);

    auto remote_packages =  //
        *neo::sqlite3::exec_tuples<std::string_view>(db.prepare(R"(
            SELECT meta_json FROM remote.crs_repo_packages
        )"_sql))
        | std::views::transform([](auto tup) -> std::optional<package_info> {
              auto [json_str] = tup;
              return dds_leaf_try->std::optional<package_info> {
                  auto meta = package_info::from_json_str(json_str);
                  if (meta.pkg_revision < 1) {
                      dds_log(warn,
                              "Remote package {} has an invalid 'pkg_revision' of {}.",
                              meta.id().to_string(),
                              meta.pkg_revision);
                      dds_log(warn, "  The corresponding package will not be available.");
                      dds_log(debug, "  The bad JSON content is: {}", json_str);
                      return std::nullopt;
                  }
                  return meta;
              }
              dds_leaf_catch(e_invalid_meta_data err) {
                  dds_log(warn, "Remote package has an invalid JSON entry: {}", err.value);
                  dds_log(warn, "  The corresponding package will not be available.");
                  dds_log(debug, "  The bad JSON content is: {}", json_str);
                  return std::nullopt;
              };
          });

    auto& update_pkg_st = db.prepare(R"(
        INSERT INTO dds_crs_packages (json, remote_id, remote_revno)
            VALUES (?1, ?2, ?3)
        ON CONFLICT(name, version, pkg_revision, remote_id) DO UPDATE
            SET json=excluded.json,
                remote_revno=?3
    )"_sql);
    int   n_updated     = 0;
    for (auto meta : remote_packages) {
        if (!meta.has_value()) {
            continue;
        }
        neo::sqlite3::exec(update_pkg_st, meta->to_json(), remote_id, remote_revno)
            .throw_if_error();
        n_updated++;
    }

    auto& delete_old_st = db.prepare(R"(
        DELETE FROM dds_crs_packages
            WHERE remote_id = ? AND remote_revno < ?
    )"_sql);
    neo::sqlite3::reset_and_bind(delete_old_st, remote_id, remote_revno).throw_if_error();
    neo::sqlite3::exec(delete_old_st).throw_if_error();
    const auto n_deleted = db.raw_database().changes();

    dds_log(debug, "Running integrity check");
    db.exec_script("PRAGMA main.integrity_check"_sql);

    dds_log(info,
            "Syncing repository .cyan[{}] Done: {} added/updated, {} deleted"_styled,
            url.to_string(),
            n_updated,
            n_deleted);
}

neo::sqlite3::connection_ref cache_db::sqlite3_db() const noexcept {
    return _db.get().raw_database();
}