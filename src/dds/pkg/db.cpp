#include "./db.hpp"

#include <dds/dym.hpp>
#include <dds/error/errors.hpp>
#include <dds/solve/solve.hpp>
#include <dds/util/log.hpp>
#include <dds/util/paths.hpp>

#include <json5/parse_data.hpp>
#include <neo/assert.hpp>
#include <neo/concepts.hpp>
#include <neo/sqlite3/exec.hpp>
#include <neo/sqlite3/iter_tuples.hpp>
#include <neo/sqlite3/single.hpp>
#include <neo/sqlite3/transaction.hpp>
#include <nlohmann/json.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/transform.hpp>

using namespace dds;

namespace nsql = neo::sqlite3;
using namespace neo::sqlite3::literals;

namespace {

void migrate_repodb_1(nsql::database& db) {
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
    )");
}

void migrate_repodb_2(nsql::database& db) {
    db.exec(R"(
        ALTER TABLE dds_cat_pkgs
            ADD COLUMN repo_transform TEXT NOT NULL DEFAULT '[]'
    )");
}

void migrate_repodb_3(nsql::database& db) {
    db.exec(R"(
        CREATE TABLE dds_cat_remotes (
            remote_id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            gen_ident TEXT NOT NULL,
            remote_url TEXT NOT NULL
        );

        CREATE TABLE dds_cat_pkgs_new (
            pkg_id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            version TEXT NOT NULL,
            description TEXT NOT NULL,
            remote_url TEXT NOT NULL,
            remote_id INTEGER
                REFERENCES dds_cat_remotes
                ON DELETE CASCADE,
            UNIQUE (name, version, remote_id)
        );

        INSERT INTO dds_cat_pkgs_new(pkg_id,
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

        CREATE TABLE dds_cat_pkg_deps_new (
            dep_id INTEGER PRIMARY KEY AUTOINCREMENT,
            pkg_id INTEGER
                NOT NULL
                REFERENCES dds_cat_pkgs_new(pkg_id)
                ON DELETE CASCADE,
            dep_name TEXT NOT NULL,
            low TEXT NOT NULL,
            high TEXT NOT NULL,
            UNIQUE(pkg_id, dep_name)
        );

        INSERT INTO dds_cat_pkg_deps_new SELECT * FROM dds_cat_pkg_deps;

        DROP TABLE dds_cat_pkg_deps;
        DROP TABLE dds_cat_pkgs;
        ALTER TABLE dds_cat_pkgs_new RENAME TO dds_cat_pkgs;
        ALTER TABLE dds_cat_pkg_deps_new RENAME TO dds_cat_pkg_deps;
    )");
}

void store_with_remote(const neo::sqlite3::statement_cache&, const pkg_info& pkg, std::monostate) {
    neo_assert_always(
        invariant,
        false,
        "There was an attempt to insert a package listing into the database where that package "
        "listing does not have a remote listing. If you see this message, it is a dds bug.",
        pkg.ident.to_string());
}

void store_with_remote(neo::sqlite3::statement_cache& stmts,
                       const pkg_info&                pkg,
                       const http_remote_listing&     http) {
    nsql::exec(  //
        stmts(R"(
            INSERT OR REPLACE INTO dds_cat_pkgs (
                name,
                version,
                remote_url,
                description
            ) VALUES (?1, ?2, ?3, ?4)
        )"_sql),
        pkg.ident.name,
        pkg.ident.version.to_string(),
        http.url,
        pkg.description);
}

void store_with_remote(neo::sqlite3::statement_cache& stmts,
                       const pkg_info&                pkg,
                       const git_remote_listing&      git) {
    std::string url = git.url;
    if (url.starts_with("https://") || url.starts_with("http://")) {
        url = "git+" + url;
    }
    if (git.auto_lib.has_value()) {
        url += "?lm=" + git.auto_lib->namespace_ + "/" + git.auto_lib->name;
    }
    url += "#" + git.ref;

    nsql::exec(  //
        stmts(R"(
            INSERT OR REPLACE INTO dds_cat_pkgs (
                name,
                version,
                remote_url,
                description
            ) VALUES (
                ?1,
                ?2,
                ?3,
                ?4
            )
        )"_sql),
        pkg.ident.name,
        pkg.ident.version.to_string(),
        url,
        pkg.description);
}

void do_store_pkg(neo::sqlite3::database&        db,
                  neo::sqlite3::statement_cache& st_cache,
                  const pkg_info&                pkg) {
    dds_log(debug, "Recording package {}@{}", pkg.ident.name, pkg.ident.version.to_string());
    std::visit([&](auto&& remote) { store_with_remote(st_cache, pkg, remote); }, pkg.remote);
    auto  db_pkg_id  = db.last_insert_rowid();
    auto& new_dep_st = st_cache(R"(
        INSERT INTO dds_cat_pkg_deps (
            pkg_id,
            dep_name,
            low,
            high
        ) VALUES (
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
        dds_log(trace, "  Depends on: {}", dep.to_string());
        nsql::exec(new_dep_st, db_pkg_id, dep.name, iv_1.low.to_string(), iv_1.high.to_string());
    }
}

void ensure_migrated(nsql::database& db) {
    db.exec(R"(
        PRAGMA foreign_keys = 1;
        CREATE TABLE IF NOT EXISTS dds_cat_meta AS
            WITH init(meta) AS (VALUES ('{"version": 0}'))
            SELECT * FROM init;
    )");
    nsql::transaction_guard tr{db};

    auto meta_st     = db.prepare("SELECT meta FROM dds_cat_meta");
    auto [meta_json] = nsql::unpack_single<std::string>(meta_st);

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

    constexpr int current_database_version = 3;

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
    meta["version"] = current_database_version;
    exec(db.prepare("UPDATE dds_cat_meta SET meta=?"), meta.dump());
}

}  // namespace

fs::path pkg_db::default_path() noexcept { return dds_data_dir() / "pkgs.db"; }

pkg_db pkg_db::open(const std::string& db_path) {
    if (db_path != ":memory:") {
        auto pardir = fs::weakly_canonical(db_path).parent_path();
        dds_log(trace, "Ensuring parent directory [{}]", pardir.string());
        fs::create_directories(pardir);
    }
    dds_log(debug, "Opening package database [{}]", db_path);
    auto db = nsql::database::open(db_path);
    try {
        ensure_migrated(db);
    } catch (const nsql::sqlite3_error& e) {
        dds_log(critical,
                "Failed to load the package database. It appears to be invalid/corrupted. The "
                "exception message is: {}",
                e.what());
        throw_external_error<errc::corrupted_catalog_db>();
    }
    dds_log(trace, "Successfully opened database");
    return pkg_db(std::move(db));
}

pkg_db::pkg_db(nsql::database db)
    : _db(std::move(db)) {}

void pkg_db::store(const pkg_info& pkg) {
    nsql::transaction_guard tr{_db};
    do_store_pkg(_db, _stmt_cache, pkg);
}

std::optional<pkg_info> pkg_db::get(const pkg_id& pk_id) const noexcept {
    auto ver_str = pk_id.version.to_string();
    dds_log(trace, "Lookup package {}@{}", pk_id.name, ver_str);
    auto& st = _stmt_cache(R"(
        SELECT
            pkg_id,
            name,
            version,
            remote_url,
            description
        FROM dds_cat_pkgs
        WHERE name = ?1 AND version = ?2
        ORDER BY pkg_id DESC
    )"_sql);
    st.reset();
    st.bindings() = std::forward_as_tuple(pk_id.name, ver_str);
    auto ec       = st.step(std::nothrow);
    if (ec == nsql::errc::done) {
        dym_target::fill([&] {
            auto all_ids = this->all();
            auto id_strings
                = ranges::views::transform(all_ids, [&](auto id) { return id.to_string(); });
            return did_you_mean(pk_id.to_string(), id_strings);
        });
        return std::nullopt;
    }
    neo_assert_always(invariant,
                      ec == nsql::errc::row,
                      "Failed to pull a package from the database",
                      ec,
                      pk_id.to_string(),
                      nsql::error_category().message(int(ec)));

    const auto& [pkg_id, name, version, remote_url, description]
        = st.row().unpack<std::int64_t, std::string, std::string, std::string, std::string>();

    ec = st.step(std::nothrow);
    if (ec == nsql::errc::row) {
        dds_log(warn,
                "There is more than one entry for package {} in the database. One will be "
                "chosen arbitrarily.",
                pk_id.to_string());
    }

    neo_assert(invariant,
               pk_id.name == name && pk_id.version == semver::version::parse(version),
               "Package metadata does not match",
               pk_id.to_string(),
               name,
               version);

    auto deps = dependencies_of(pk_id);

    auto info = pkg_info{
        pk_id,
        std::move(deps),
        std::move(description),
        parse_remote_url(remote_url),
    };

    return info;
}

auto pair_to_pkg_id = [](auto&& pair) {
    const auto& [name, ver] = pair;
    return pkg_id{name, semver::version::parse(ver)};
};

std::vector<pkg_id> pkg_db::all() const noexcept {
    return nsql::exec_tuples<std::string, std::string>(
               _stmt_cache("SELECT name, version FROM dds_cat_pkgs"_sql))
        | neo::lref                                 //
        | ranges::views::transform(pair_to_pkg_id)  //
        | ranges::to_vector;
}

std::vector<pkg_id> pkg_db::by_name(std::string_view sv) const noexcept {
    return nsql::exec_tuples<std::string, std::string>(  //
               _stmt_cache(
                   R"(
                SELECT name, version
                  FROM dds_cat_pkgs
                 WHERE name = ?
                 ORDER BY pkg_id DESC
                )"_sql),
               sv)                                  //
        | neo::lref                                 //
        | ranges::views::transform(pair_to_pkg_id)  //
        | ranges::to_vector;
}

std::vector<dependency> pkg_db::dependencies_of(const pkg_id& pkg) const noexcept {
    dds_log(trace, "Lookup dependencies of {}@{}", pkg.name, pkg.version.to_string());
    return nsql::exec_tuples<std::string,
                             std::string,
                             std::string>(  //
               _stmt_cache(
                   R"(
                WITH this_pkg_id AS (
                    SELECT pkg_id
                      FROM dds_cat_pkgs
                     WHERE name = ? AND version = ?
                )
                SELECT dep_name, low, high
                  FROM dds_cat_pkg_deps
                 WHERE pkg_id IN this_pkg_id
              ORDER BY dep_name
                )"_sql),
               pkg.name,
               pkg.version.to_string())  //
        | neo::lref                      //
        | ranges::views::transform([](auto&& pair) {
               auto& [name, low, high] = pair;
               auto dep
                   = dependency{name, {semver::version::parse(low), semver::version::parse(high)}};
               dds_log(trace, "  Depends: {}", dep.to_string());
               return dep;
           })  //
        | ranges::to_vector;
}
