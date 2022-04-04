#include "./cache_db.hpp"

#include "./error.hpp"

#include <bpt/error/handle.hpp>
#include <bpt/error/on_error.hpp>
#include <bpt/error/result.hpp>
#include <bpt/error/try_catch.hpp>
#include <bpt/temp.hpp>
#include <bpt/util/compress.hpp>
#include <bpt/util/db/migrate.hpp>
#include <bpt/util/db/query.hpp>
#include <bpt/util/fs/path.hpp>
#include <bpt/util/http/pool.hpp>
#include <bpt/util/http/response.hpp>
#include <bpt/util/log.hpp>
#include <bpt/util/string.hpp>
#include <bpt/util/url.hpp>

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

#include <charconv>

using namespace bpt;
using namespace bpt::crs;
using namespace neo::sqlite3::literals;
using namespace fansi::literals;
using std::optional;
using std::string;
using std::string_view;
namespace chrono = std::chrono;
using chrono::steady_clock;
using steady_time_point = steady_clock::time_point;

neo::sqlite3::statement& cache_db::_prepare(neo::sqlite3::sql_string_literal sql) const {
    return _db.get().prepare(sql);
}

cache_db cache_db::open(unique_database& db) {
    bpt::apply_db_migrations(db, "bpt_crs_meta", [](auto& db) {
        db.exec_script(R"(
            CREATE TABLE bpt_crs_remotes (
                remote_id INTEGER PRIMARY KEY,
                url TEXT NOT NULL,
                unique_name TEXT NOT NULL UNIQUE,
                revno INTEGER NOT NULL,
                -- HTTP Etag header
                etag TEXT,
                -- HTTP Last-Modified header
                last_modified TEXT,
                -- System time of the most recent DB update
                resource_time INTEGER,
                -- Content of the prior Cache-Control header for HTTP remotes
                cache_control TEXT
            );

            CREATE TABLE bpt_crs_packages (
                pkg_id INTEGER PRIMARY KEY,
                json TEXT NOT NULL,
                remote_id INTEGER NOT NULL
                    REFERENCES bpt_crs_remotes
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
                pkg_version INTEGER NOT NULL
                    GENERATED ALWAYS
                    AS (json_extract(json, '$.pkg-version'))
                    STORED,
                UNIQUE (name, version, pkg_version, remote_id)
            );

            CREATE TABLE bpt_crs_libraries (
                lib_id INTEGER PRIMARY KEY,
                pkg_id INTEGER NOT NULL
                    REFERENCES bpt_crs_packages
                    ON DELETE CASCADE,
                name TEXT NOT NULL,
                path TEXT NOT NULL,
                UNIQUE (pkg_id, name)
            );

            CREATE TRIGGER bpt_crs_libraries_auto_insert
                AFTER INSERT ON bpt_crs_packages
                FOR EACH ROW
                BEGIN
                    INSERT INTO bpt_crs_libraries (pkg_id, name, path)
                        WITH libraries AS (
                            SELECT value FROM json_each(new.json, '$.libraries')
                        )
                        SELECT
                            new.pkg_id,
                            json_extract(lib.value, '$.name'),
                            json_extract(lib.value, '$.path')
                        FROM libraries AS lib;
                END;

            CREATE TRIGGER bpt_crs_libraries_auto_update
                AFTER UPDATE ON bpt_crs_packages
                FOR EACH ROW WHEN new.json != old.json
                BEGIN
                    DELETE FROM bpt_crs_libraries
                        WHERE pkg_id = new.pkg_id;
                    INSERT INTO bpt_crs_libraries (pkg_id, name, path)
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
        CREATE TEMPORARY TABLE IF NOT EXISTS bpt_crs_enabled_remotes (
            enablement_id INTEGER PRIMARY KEY,
            remote_id INTEGER NOT NULL -- references main.bpt_crs_remotes
                UNIQUE ON CONFLICT IGNORE
        );
        CREATE TEMPORARY VIEW IF NOT EXISTS enabled_packages AS
            SELECT * FROM bpt_crs_packages
            JOIN bpt_crs_enabled_remotes USING (remote_id)
            ORDER BY enablement_id ASC
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

void cache_db::forget_all() { db_exec(_prepare("DELETE FROM bpt_crs_remotes"_sql)).value(); }

namespace {

neo::any_input_range<cache_db::package_entry>
cache_entries_for_query(neo::sqlite3::statement&& st) {
    return bpt_leaf_try->result<neo::any_input_range<cache_db::package_entry>> {
        auto pin = neo::copy_shared(std::move(st));
        return neo::sqlite3::iter_tuples<std::int64_t, std::int64_t, string_view>(*pin)
            | std::views::transform([pin](auto row) { return pkg_entry_from_row(row); })  //
            ;
    }
    bpt_leaf_catch(catch_<neo::sqlite3::error> err)->noreturn_t {
        BOOST_LEAF_THROW_EXCEPTION(err, err.matched.code());
    }
    bpt_leaf_catch_all->noreturn_t {
        neo_assert(invariant,
                   false,
                   "Unexpected exception while loading from CRS cache database",
                   diagnostic_info);
    };
}

}  // namespace

neo::any_input_range<cache_db::package_entry>
cache_db::for_package(bpt::name const& name, semver::version const& version) const {
    neo_assertion_breadcrumbs("Loading package cache entries for name+version",
                              name.str,
                              version.to_string());
    auto st       = *_db.get().sqlite3_db().prepare(R"(
        SELECT pkg_id, remote_id, json
        FROM enabled_packages
        WHERE name = ? AND version = ?
    )");
    st.bindings() = std::forward_as_tuple(name.str, version.to_string());
    return cache_entries_for_query(std::move(st));
}

neo::any_input_range<cache_db::package_entry> cache_db::for_package(bpt::name const& name) const {
    neo_assertion_breadcrumbs("Loading package cache entries for name", name.str);
    auto st       = *_db.get().sqlite3_db().prepare(R"(
        SELECT pkg_id, remote_id, json
        FROM enabled_packages
        WHERE name = ?
    )");
    st.bindings() = std::forward_as_tuple(name.str);
    db_bind(st, string_view(name.str)).value();
    return cache_entries_for_query(std::move(st));
}

neo::any_input_range<cache_db::package_entry> cache_db::all_enabled() const {
    neo_assertion_breadcrumbs("Loading all enabled package entries");
    auto st
        = *_db.get().sqlite3_db().prepare("SELECT pkg_id, remote_id, json FROM enabled_packages");
    return cache_entries_for_query(std::move(st));
}

optional<cache_db::remote_entry> cache_db::get_remote(neo::url_view const& url_) const {
    return bpt_leaf_try->optional<cache_db::remote_entry> {
        auto url = url_.normalized();
        auto row = db_single<std::int64_t, string, string>(  //
                       _prepare(R"(
                            SELECT remote_id, url, unique_name
                            FROM bpt_crs_remotes
                            WHERE url = ?
                       )"_sql),
                       string_view(url.to_string()))
                       .value();
        auto [rowid, url_str, name] = row;
        return cache_db::remote_entry{rowid, bpt::parse_url(url_str), name};
    }
    bpt_leaf_catch(matchv<neo::sqlite3::errc::done>) { return std::nullopt; };
}

optional<cache_db::remote_entry> cache_db::get_remote_by_id(std::int64_t rowid) const {
    auto row = neo::sqlite3::one_row<string, string>(  //
        _prepare(R"(
            SELECT url, unique_name
            FROM bpt_crs_remotes WHERE remote_id = ?
        )"_sql),
        rowid);
    if (row.has_value()) {
        auto [url_str, name] = *row;
        return remote_entry{rowid, bpt::parse_url(url_str), name};
    }
    return std::nullopt;
}

void cache_db::enable_remote(neo::url_view const& url_) {
    auto url = url_.normalized();
    auto res = neo::sqlite3::one_row(  //
        _prepare(R"(
            INSERT INTO bpt_crs_enabled_remotes (remote_id)
            SELECT remote_id
              FROM bpt_crs_remotes
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
    fs::path                         local_path;
    std::optional<std::string_view>  etag;
    std::optional<std::string_view>  last_modified;
    std::optional<steady_time_point> resource_time;
    std::optional<std::string_view>  cache_control;
    bool                             up_to_date;
};

/**
 * @brief Determine whether we should revalidate a cached resource based on the cache-control and
 * age of the resource.
 *
 * @param cache_control The 'Cache-Control' header
 * @param resource_time The create-time of the resource. This may be affected by the 'Age' header.
 */
bool should_revalidate(std::string_view cache_control, steady_time_point resource_time) {
    auto parts = bpt::split_view(cache_control, ", ");
    if (std::ranges::find(parts, "no-cache", trim_view) != parts.end()) {
        // Always revalidate
        return true;
    }
    if (auto max_age = std::ranges::find_if(parts, NEO_TL(_1.starts_with("max-age=")));
        max_age != parts.end()) {
        auto age_str     = bpt::trim_view(max_age->substr(std::strlen("max-age=")));
        int  max_age_int = 0;
        auto r = std::from_chars(age_str.data(), age_str.data() + age_str.size(), max_age_int);
        if (r.ec != std::errc{}) {
            bpt_log(warn,
                    "Malformed max-age integer '{}' in stored cache-control header '{}'",
                    age_str,
                    cache_control);
            return true;
        }
        auto stale_time = resource_time + chrono::seconds(max_age_int);
        if (stale_time < steady_clock::now()) {
            // The associated resource has become stale, so it must revalidate
            return true;
        } else {
            // The cached item is still fresh
            bpt_log(debug, "Cached repo data is still within its max-age.");
            bpt_log(debug,
                    "Repository data will expire in {} seconds",
                    chrono::duration_cast<chrono::seconds>(stale_time - steady_clock::now())
                        .count());
            return false;
        }
    }
    // No other headers supported yet. Just revalidate.
    return true;
}

neo::co_resource<remote_repo_db_info> get_remote_db(bpt::unique_database& db, neo::url url) {
    if (url.scheme == "file") {
        auto path = fs::path(url.path);
        bpt_log(info, "Importing local repository .cyan[{}] ..."_styled, url.path);
        auto info = remote_repo_db_info{
            .local_path    = path / "repo.db",
            .etag          = std::nullopt,
            .last_modified = std::nullopt,
            .resource_time = std::nullopt,
            .cache_control = std::nullopt,
            .up_to_date    = false,
        };
        co_yield info;
    } else {
        bpt::http_request_params params;
        auto&                    prio_info_st = db.prepare(
            "SELECT etag, last_modified, resource_time, cache_control "
            "FROM bpt_crs_remotes "
            "WHERE url=?"_sql);
        auto url_str = url.to_string();
        neo_assertion_breadcrumbs("Pulling remote repository metadata", url_str);
        bpt_log(debug, "Syncing repository [{}] via HTTP", url_str);
        neo::sqlite3::reset_and_bind(prio_info_st, std::string_view(url_str)).throw_if_error();
        auto prior_info = neo::sqlite3::one_row<std::optional<std::string>,
                                                std::optional<std::string>,
                                                std::int64_t,
                                                std::optional<std::string>>(prio_info_st);
        if (prior_info.has_value()) {
            bpt_log(debug, "Seen this remote repository before. Checking for updates.");
            const auto& [etag, last_mod, prev_rc_time_, cache_control] = *prior_info;
            if (etag.has_value()) {
                params.prior_etag = *etag;
            }
            if (last_mod.has_value()) {
                params.last_modified = *last_mod;
            }
            // Check if the cached item is stale according to the server.
            const auto prev_rc_time = steady_time_point(steady_clock::duration(prev_rc_time_));
            if (cache_control and not should_revalidate(*cache_control, prev_rc_time)) {
                bpt_log(info, "Repository data from .cyan[{}] is fresh"_styled, url_str);
                auto info = remote_repo_db_info{
                    .local_path    = "",
                    .etag          = etag,
                    .last_modified = last_mod,
                    .resource_time = prev_rc_time,
                    .cache_control = cache_control,
                    .up_to_date    = true,
                };
                co_yield info;
                co_return;
            }
        }

        if (!prior_info.has_value() && prior_info.errc() != neo::sqlite3::errc::done) {
            prior_info.throw_error();
        }

        auto& pool = bpt::http_pool::thread_local_pool();

        auto repo_db_gz_url = url / "repo.db.gz";

        bool           do_discard = true;
        request_result rinfo      = pool.request(repo_db_gz_url, params);
        neo_defer {
            if (do_discard) {
                rinfo.client.abort_client();
            }
        };

        if (auto message = rinfo.resp.header_value("x-bpt-user-message")) {
            bpt_log(info, "Message from repository [{}]: {}", url_str, *message);
        }

        // Compute the create-time of the source. By default, just the request time.
        auto resource_time = steady_clock::now();
        if (auto age_str = rinfo.resp.header_value("Age")) {
            int  age_int = 0;
            auto r = std::from_chars(age_str->data(), age_str->data() + age_str->size(), age_int);
            if (r.ec == std::errc{}) {
                resource_time -= chrono::seconds{age_int};
            }
        }

        // Init the info about the resource that we will return to the caller
        remote_repo_db_info yield_info{
            .local_path    = "",
            .etag          = rinfo.resp.etag(),
            .last_modified = rinfo.resp.last_modified(),
            .resource_time = resource_time,
            .cache_control = rinfo.resp.header_value("Cache-Control"),
            .up_to_date    = false,
        };

        if (rinfo.resp.not_modified()) {
            bpt_log(info, "Repository data from .cyan[{}] is up-to-date"_styled, url_str);
            do_discard = false;
            rinfo.discard_body();
            yield_info.up_to_date = true;
            co_yield yield_info;
            co_return;
        }

        bpt_log(info, "Syncing repository .cyan[{}] ..."_styled, url_str);

        // pool.request() will resolve redirects and errors
        neo_assert(invariant,
                   !rinfo.resp.is_redirect(),
                   "Did not expect an HTTP redirect at this IO layer");
        neo_assert(invariant,
                   !rinfo.resp.is_error(),
                   "Did not expect an HTTP error at this IO layer");

        auto tmpdir    = bpt::temporary_dir::create();
        auto dest_file = tmpdir.path() / "repo.db.gz";
        std::filesystem::create_directories(tmpdir.path());
        rinfo.save_file(dest_file);
        do_discard = false;

        auto repo_db = tmpdir.path() / "repo.db";
        bpt::decompress_file_gz(dest_file, repo_db).value();
        yield_info.local_path = repo_db;
        co_yield yield_info;
    }
}

}  // namespace

void cache_db::sync_remote(const neo::url_view& url_) const {
    bpt::unique_database& db  = _db;
    auto                  url = url_.normalized();
    BPT_E_SCOPE(e_sync_remote{url});
    auto remote_db = get_remote_db(_db, url);

    auto rc_time = remote_db->resource_time;

    if (remote_db->up_to_date) {
        neo::sqlite3::exec(  //
            db.prepare("UPDATE bpt_crs_remotes "
                       "SET resource_time = ?1, cache_control = ?2"_sql),
            rc_time ? std::make_optional(rc_time->time_since_epoch().count()) : std::nullopt,
            remote_db->cache_control)
            .throw_if_error();
        return;
    }

    neo::sqlite3::exec(db.prepare("ATTACH DATABASE ? AS remote"_sql),
                       remote_db->local_path.string())
        .throw_if_error();
    neo_defer { db.exec_script(R"(DETACH DATABASE remote)"_sql); };

    // Import those packages
    neo::sqlite3::transaction_guard tr{db.sqlite3_db()};

    auto& update_remote_st         = db.prepare(R"(
        INSERT INTO bpt_crs_remotes
            (url, unique_name, revno, etag, last_modified, resource_time, cache_control)
        VALUES (
            ?1,  -- url
            (SELECT name FROM remote.crs_repo_self), -- unique_name
            1,   -- revno
            ?2,  -- etag
            ?3,  -- last_modified
            ?4,  -- resource_time
            ?5   -- Cache-Control header
        )
        ON CONFLICT (unique_name) DO UPDATE
            SET url = ?1,
                etag = ?2,
                last_modified = ?3,
                resource_time = ?4,
                cache_control = ?5,
                revno = revno + 1
        RETURNING remote_id, revno
    )"_sql);
    auto [remote_id, remote_revno] = *neo::sqlite3::one_row<std::int64_t, std::int64_t>(  //
        update_remote_st,
        url.to_string(),
        remote_db->etag,
        remote_db->last_modified,
        rc_time ? std::make_optional(rc_time->time_since_epoch().count()) : std::nullopt,
        remote_db->cache_control);

    auto remote_packages =  //
        *neo::sqlite3::exec_tuples<std::string_view>(db.prepare(R"(
            SELECT meta_json FROM remote.crs_repo_packages
        )"_sql))
        | std::views::transform([](auto tup) -> std::optional<package_info> {
              auto [json_str] = tup;
              return bpt_leaf_try->std::optional<package_info> {
                  auto meta = package_info::from_json_str(json_str);
                  if (meta.id.revision < 1) {
                      bpt_log(warn,
                              "Remote package {} has an invalid 'pkg-version' of {}.",
                              meta.id.to_string(),
                              meta.id.revision);
                      bpt_log(warn, "  The corresponding package will not be available.");
                      bpt_log(debug, "  The bad JSON content is: {}", json_str);
                      return std::nullopt;
                  }
                  return meta;
              }
              bpt_leaf_catch(e_invalid_meta_data err) {
                  bpt_log(warn, "Remote package has an invalid JSON entry: {}", err.value);
                  bpt_log(warn, "  The corresponding package will not be available.");
                  bpt_log(debug, "  The bad JSON content is: {}", json_str);
                  return std::nullopt;
              };
          });

    auto n_before = *neo::sqlite3::one_cell<std::int64_t>(
        db.prepare("SELECT count(*) FROM bpt_crs_packages"_sql));
    auto& update_pkg_st = db.prepare(R"(
        INSERT INTO bpt_crs_packages (json, remote_id, remote_revno)
            VALUES (?1, ?2, ?3)
        ON CONFLICT(name, version, pkg_version, remote_id) DO UPDATE
            SET json=excluded.json,
                remote_revno=?3
    )"_sql);
    for (auto meta : remote_packages) {
        if (!meta.has_value()) {
            continue;
        }
        neo::sqlite3::exec(update_pkg_st, meta->to_json(), remote_id, remote_revno)
            .throw_if_error();
    }
    auto n_after = *neo::sqlite3::one_cell<std::int64_t>(
        db.prepare("SELECT count(*) FROM bpt_crs_packages"_sql));
    const std::int64_t n_added = n_after - n_before;

    auto& delete_old_st = db.prepare(R"(
        DELETE FROM bpt_crs_packages
            WHERE remote_id = ? AND remote_revno < ?
    )"_sql);
    neo::sqlite3::reset_and_bind(delete_old_st, remote_id, remote_revno).throw_if_error();
    neo::sqlite3::exec(delete_old_st).throw_if_error();
    const auto n_deleted = db.sqlite3_db().changes();

    bpt_log(debug, "Running integrity check");
    db.exec_script("PRAGMA main.integrity_check"_sql);

    bpt_log(info,
            "Syncing repository .cyan[{}] Done: {} added, {} deleted"_styled,
            url.to_string(),
            n_added,
            n_deleted);
}

neo::sqlite3::connection_ref cache_db::sqlite3_db() const noexcept {
    return _db.get().sqlite3_db();
}