#include "./cache_db.hpp"

#include <dds/error/handle.hpp>
#include <dds/error/result.hpp>
#include <dds/error/try_catch.hpp>
#include <dds/util/db/migrate.hpp>
#include <dds/util/db/query.hpp>
#include <dds/util/fs/path.hpp>

#include <boost/leaf.hpp>
#include <neo/ranges.hpp>
#include <neo/tl.hpp>

using namespace dds;
using namespace dds::crs;
using namespace neo::sqlite3::literals;
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
                revno INTEGER NOT NULL
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
                path TEXT NOT NULL UNIQUE,
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
                FOR EACH ROW
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

            CREATE TABLE dds_crs_library_usages (
                usage_id INTEGER PRIMARY KEY,
                lib_id INTEGER NOT NULL
                    REFERENCES dds_crs_libraries
                    ON DELETE CASCADE,
                uses_name TEXT NOT NULL,
                kind TEXT NOT NULL,
                UNIQUE (lib_id, uses_name)
            );

            CREATE TRIGGER dds_crs_library_usages_auto_insert
                AFTER INSERT ON dds_crs_libraries
                FOR EACH ROW
                BEGIN
                    INSERT INTO dds_crs_library_usages
                            (lib_id, uses_name, kind)
                        SELECT
                            new.lib_id,
                            json_extract(use.value, '$.lib'),
                            json_extract(use.value, '$.for')
                        FROM
                            dds_crs_packages AS pkgs,
                            json_each(pkgs.json, '$.libraries') AS libs,
                            json_each(libs.value, '$.uses') AS use
                        WHERE
                            pkgs.pkg_id = new.pkg_id
                            AND json_extract(libs.value, '$.name') = new.name;
                END;

            CREATE TRIGGER dds_crs_library_usages_auto_update
                AFTER UPDATE ON dds_crs_libraries
                FOR EACH ROW
                BEGIN
                    DELETE FROM dds_crs_library_usages
                        WHERE lib_id = new.lib_id;
                    INSERT INTO dds_crs_library_usages
                            (lib_id, uses_name, kind)
                        SELECT
                            new.lib_id,
                            json_extract(use.value, '$.lib'),
                            json_extract(use.value, '$.for')
                        FROM
                            dds_crs_packages AS pkgs,
                            json_each(pkgs.json, '$.libraries') AS libs,
                            json_each(libs.value, '$.uses') AS use
                        WHERE
                            pkgs.pkg_id = new.pkg_id
                            AND json_extract(libs.value, '$.name') = new.name;
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
