#include "./db.hpp"

#include <dds/dym.hpp>
#include <dds/error/errors.hpp>
#include <dds/error/nonesuch.hpp>
#include <dds/solve/solve.hpp>
#include <dds/util/env.hpp>
#include <dds/util/log.hpp>
#include <dds/util/paths.hpp>

#include <json5/parse_data.hpp>
#include <neo/assert.hpp>
#include <neo/concepts.hpp>
#include <neo/sqlite3/error.hpp>
#include <neo/sqlite3/exec.hpp>
#include <neo/sqlite3/iter_tuples.hpp>
#include <neo/sqlite3/transaction.hpp>
#include <nlohmann/json.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/transform.hpp>

using namespace dds;
namespace nsql = neo::sqlite3;
using namespace nsql::literals;

namespace dds {

void add_init_repo(nsql::connection_ref db) noexcept;

}  // namespace dds

namespace {

void migrate_repodb_1(nsql::connection& db) {
    db.exec(R"(
        CREATE TABLE dds_cat_pkgs (
            pkg_id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            version TEXT NOT NULL,
            git_url TEXT,
            git_ref TEXT,
            lm_name TEXT,
            lm_namespace TEXT,
            description TEXT NOT NULL,
            UNIQUE(name, version),
            CONSTRAINT has_source_info CHECK(
                (
                    git_url NOT NULL
                    AND git_ref NOT NULL
                )
                = 1
            ),
            CONSTRAINT valid_lm_info CHECK(
                (
                    lm_name NOT NULL
                    AND lm_namespace NOT NULL
                )
                +
                (
                    lm_name ISNULL
                    AND lm_namespace ISNULL
                )
                = 1
            )
        );

        CREATE TABLE dds_cat_pkg_deps (
            dep_id INTEGER PRIMARY KEY AUTOINCREMENT,
            pkg_id INTEGER NOT NULL REFERENCES dds_cat_pkgs(pkg_id),
            dep_name TEXT NOT NULL,
            low TEXT NOT NULL,
            high TEXT NOT NULL,
            UNIQUE(pkg_id, dep_name)
        );
    )")
        .throw_if_error();
}

void migrate_repodb_2(nsql::connection& db) {
    db.exec(R"(
        ALTER TABLE dds_cat_pkgs
            ADD COLUMN repo_transform TEXT NOT NULL DEFAULT '[]'
    )")
        .throw_if_error();
}

void migrate_repodb_3(nsql::connection& db) {
    db.exec(R"(
        CREATE TABLE dds_pkg_remotes (
            remote_id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            url TEXT NOT NULL,
            db_etag TEXT,
            db_mtime TEXT
        );

        CREATE TABLE dds_pkgs (
            pkg_id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            version TEXT NOT NULL,
            description TEXT NOT NULL,
            remote_url TEXT NOT NULL,
            remote_id INTEGER
                REFERENCES dds_pkg_remotes
                ON DELETE CASCADE,
            UNIQUE (name, version, remote_id)
        );

        INSERT INTO dds_pkgs(pkg_id,
                             name,
                             version,
                             description,
                             remote_url)
            SELECT pkg_id,
                   name,
                   version,
                   description,
                   'git+' || git_url || (
                       CASE
                         WHEN lm_name ISNULL THEN ''
                         ELSE ('?lm=' || lm_namespace || '/' || lm_name)
                       END
                   ) || '#' || git_ref
            FROM dds_cat_pkgs;

        CREATE TABLE dds_pkg_deps (
            dep_id INTEGER PRIMARY KEY AUTOINCREMENT,
            pkg_id INTEGER
                NOT NULL
                REFERENCES dds_pkgs(pkg_id)
                ON DELETE CASCADE,
            dep_name TEXT NOT NULL,
            low TEXT NOT NULL,
            high TEXT NOT NULL,
            UNIQUE(pkg_id, dep_name)
        );

        INSERT INTO dds_pkg_deps SELECT * FROM dds_cat_pkg_deps;

        DROP TABLE dds_cat_pkg_deps;
        DROP TABLE dds_cat_pkgs;
    )")
        .throw_if_error();
}

void migrate_repodb_4(nsql::connection_ref db) {
    db.exec(R"(
        CREATE TABLE dds_pkg_deps_1 (
            dep_id INTEGER PRIMARY KEY,
            pkg_id INTEGER NOT NULL
                REFERENCES dds_pkgs(pkg_id)
                ON DELETE CASCADE,
            dep_name TEXT NOT NULL,
            low TEXT NOT NULL,
            high TEXT NOT NULL,
            kind TEXT NOT NULL,
            UNIQUE(pkg_id, dep_name, kind)
        );
        INSERT INTO dds_pkg_deps_1
            SELECT dep_id, pkg_id, dep_name, low, high, 'lib'
            FROM dds_pkg_deps;
        DROP TABLE dds_pkg_deps;
        ALTER TABLE dds_pkg_deps_1 RENAME TO dds_pkg_deps;
        CREATE INDEX idx_deps_for_pkg ON dds_pkg_deps (pkg_id);
    )")
        .throw_if_error();
}

void do_store_pkg(neo::sqlite3::connection&      db,
                  neo::sqlite3::statement_cache& st_cache,
                  const pkg_listing&             pkg) {
    dds_log(debug, "Recording package {}@{}", pkg.ident.name.str, pkg.ident.version.to_string());
    auto& store_pkg_st = st_cache(R"(
        INSERT OR REPLACE INTO dds_pkgs
            (name, version, remote_url, description)
        VALUES
            (?, ?, ?, ?)
    )"_sql);
    nsql::exec(store_pkg_st,
               pkg.ident.name.str,
               pkg.ident.version.to_string(),
               pkg.remote_pkg.to_url_string(),
               pkg.description)
        .throw_if_error();

    auto  db_pkg_id  = db.last_insert_rowid();
    auto& new_dep_st = st_cache(R"(
        INSERT INTO dds_pkg_deps (
            pkg_id,
            dep_name,
            low,
            high,
            kind
        ) VALUES (
            ?,
            ?,
            ?,
            ?,
            ?
        )
    )"_sql);
    for (const auto& dep : pkg.deps) {
        new_dep_st.reset();
        assert(dep.versions.num_intervals() == 1);
        auto iv_1 = *dep.versions.iter_intervals().begin();
        dds_log(trace, "  Depends on: {}", dep.decl_to_string());
        nsql::exec(new_dep_st,
                   db_pkg_id,
                   dep.name.str,
                   iv_1.low.to_string(),
                   iv_1.high.to_string(),
                   for_kind_str(dep.for_kind))
            .throw_if_error();
    }
}

void ensure_migrated(nsql::connection& db) {
    db.exec(R"(
        PRAGMA foreign_keys = 1;
        CREATE TABLE IF NOT EXISTS dds_cat_meta AS
            WITH init(meta) AS (VALUES ('{"version": 0}'))
            SELECT * FROM init;
    )")
        .throw_if_error();
    nsql::transaction_guard tr{db};

    auto meta_json = *nsql::one_cell<std::string>(*db.prepare("SELECT meta FROM dds_cat_meta"));

    auto meta = nlohmann::json::parse(meta_json);
    if (!meta.is_object()) {
        dds_log(critical, "Root of database dds_cat_meta cell should be a JSON object");
        throw_external_error<errc::corrupted_catalog_db>();
    }

    auto version_ = meta["version"];
    if (!version_.is_number_integer()) {
        dds_log(critical, "'version' key in dds_cat_meta is not an integer");
        throw_external_error<errc::corrupted_catalog_db>(
            "The database metadata is invalid [bad dds_meta.version]");
    }

    constexpr int current_database_version = 4;

    int version = version_;

    if (version > current_database_version) {
        dds_log(critical,
                "Catalog version is {}, but we only support up to {}",
                version,
                current_database_version);
        throw_external_error<errc::catalog_too_new>();
    }

    if (version < 1) {
        dds_log(debug, "Applying pkg_db migration 1");
        migrate_repodb_1(db);
    }
    if (version < 2) {
        dds_log(debug, "Applying pkg_db migration 2");
        migrate_repodb_2(db);
    }
    if (version < 3) {
        dds_log(debug, "Applying pkg_db migration 3");
        migrate_repodb_3(db);
    }
    if (version < 4) {
        dds_log(debug, "Applying pkg_db migration 4");
        migrate_repodb_4(db);
    }
    meta["version"] = current_database_version;
    exec(*db.prepare("UPDATE dds_cat_meta SET meta=?"), meta.dump()).throw_if_error();
    tr.commit();

    if (version < 3 && !getenv_bool("DDS_NO_ADD_INITIAL_REPO")) {
        // Version 3 introduced remote repositories. If we're updating to 3, add that repo now
        dds_log(info, "Downloading initial repository");
        dds::add_init_repo(db);
    }
}

}  // namespace

fs::path pkg_db::default_path() noexcept { return dds_data_dir() / "pkgs.db"; }

pkg_db pkg_db::open(const std::string& db_path) {
    if (db_path != ":memory:") {
        auto pardir = fs::weakly_canonical(db_path).parent_path();
        fs::create_directories(pardir);
    }
    dds_log(debug, "Opening package database [{}]", db_path);
    auto db = *nsql::connection::open(db_path);
    try {
        ensure_migrated(db);
    } catch (const nsql::error& e) {
        dds_log(critical,
                "Failed to load the package database. It appears to be invalid/corrupted. The "
                "exception message is: {}",
                e.what());
        throw_external_error<errc::corrupted_catalog_db>();
    }
    dds_log(trace, "Successfully opened database");
    return pkg_db(std::move(db));
}

pkg_db::pkg_db(nsql::connection db)
    : _db(std::move(db)) {}

void pkg_db::store(const pkg_listing& pkg) {
    nsql::transaction_guard tr{_db};
    do_store_pkg(_db, _stmt_cache, pkg);
}

result<pkg_listing> pkg_db::get(const pkg_id& pk_id) const noexcept {
    auto ver_str = pk_id.version.to_string();
    dds_log(trace, "Lookup package {}@{}", pk_id.name.str, ver_str);
    auto& st = _stmt_cache(R"(
        SELECT
            pkg_id,
            name,
            version,
            remote_url,
            description
        FROM dds_pkgs
        WHERE name = ?1 AND version = ?2
        ORDER BY pkg_id DESC
    )"_sql);
    nsql::reset_and_bind(st, pk_id.name.str, ver_str).throw_if_error();
    auto res = st.step();
    if (res == nsql::errc::done) {
        return new_error([&] {
            auto all_ids = this->all();
            auto id_strings
                = ranges::views::transform(all_ids, [&](auto id) { return id.to_string(); });
            return e_nonesuch{pk_id.to_string(), did_you_mean(pk_id.to_string(), id_strings)};
        });
    }
    neo_assert_always(invariant,
                      res == nsql::errc::row,
                      "Failed to pull a package from the database",
                      res.errc(),
                      pk_id.to_string(),
                      nsql::error_category().message(int(res.errc())));

    const auto& [pkg_id, name, version, remote_url, description]
        = st.row().unpack<std::int64_t, std::string, std::string, std::string, std::string>();

    res = st.step();
    if (res == nsql::errc::row) {
        dds_log(warn,
                "There is more than one entry for package {} in the database. One will be "
                "chosen arbitrarily.",
                pk_id.to_string());
    }

    neo_assert(invariant,
               pk_id.name.str == name && pk_id.version == semver::version::parse(version),
               "Package metadata does not match",
               pk_id.to_string(),
               name,
               version);

    auto deps = dependencies_of(pk_id);

    auto info = pkg_listing{
        .ident       = pk_id,
        .deps        = std::move(deps),
        .description = std::move(description),
        .remote_pkg  = any_remote_pkg::from_url(neo::url::parse(remote_url)),
    };

    return info;
}

auto pair_to_pkg_id = [](auto&& pair) {
    const auto& [name, ver] = pair;
    return pkg_id{name, semver::version::parse(ver)};
};

std::vector<pkg_id> pkg_db::all() const noexcept {
    return *nsql::exec_tuples<std::string,
                              std::string>(_stmt_cache("SELECT name, version FROM dds_pkgs"_sql))
        | std::views::transform(pair_to_pkg_id)  //
        | ranges::to_vector;
}

std::vector<pkg_id> pkg_db::by_name(std::string_view sv) const noexcept {
    return *nsql::exec_tuples<std::string, std::string>(  //
               _stmt_cache(
                   R"(
                SELECT name, version
                  FROM dds_pkgs
                 WHERE name = ?
                 ORDER BY pkg_id DESC
                )"_sql),
               sv)
        | std::views::transform(pair_to_pkg_id)  //
        | ranges::to_vector;
}

std::vector<dependency> pkg_db::dependencies_of(const pkg_id& pkg) const noexcept {
    dds_log(trace, "Lookup dependencies of {}", pkg.to_string());
    auto ver_str = pkg.version.to_string();
    return *nsql::exec_tuples<std::string,
                              std::string,
                              std::string,
                              std::string>(  //
               _stmt_cache(
                   R"(
                WITH this_pkg_id AS (
                    SELECT pkg_id
                      FROM dds_pkgs
                     WHERE name = ? AND version = ?
                )
                SELECT dep_name, low, high, kind
                  FROM dds_pkg_deps
                 WHERE pkg_id IN this_pkg_id
              ORDER BY dep_name
                )"_sql),
               pkg.name.str,
               ver_str)
        | std::views::transform([](auto&& tup) {
              auto& [name, low, high, kind_] = tup;

              auto kind = for_kind_from_str(kind_);
              if (!kind) {
                  dds_log(warn, "Unknown dep-for-kind '{}' (Defaulting to 'lib')", kind_);
              }
              auto dep = dependency{name,
                                    {semver::version::parse(low), semver::version::parse(high)},
                                    kind.value_or(dep_for_kind::lib)};
              dds_log(trace, "  Depends: {}", dep.decl_to_string());
              return dep;
          })  //
        | ranges::to_vector;
}
