#include "./catalog.hpp"

#include "./import.hpp"

#include <dds/dym.hpp>
#include <dds/error/errors.hpp>
#include <dds/solve/solve.hpp>
#include <dds/util/log.hpp>

#include <json5/parse_data.hpp>
#include <neo/assert.hpp>
#include <neo/concepts.hpp>
#include <neo/sqlite3/exec.hpp>
#include <neo/sqlite3/iter_tuples.hpp>
#include <neo/sqlite3/single.hpp>
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
            repo_transform TEXT NOT NULL DEFAULT '[]',
            UNIQUE (name, version, remote_id)
        );

        INSERT INTO dds_cat_pkgs_new(pkg_id,
                                     name,
                                     version,
                                     description,
                                     remote_url,
                                     repo_transform)
            SELECT pkg_id,
                   name,
                   version,
                   description,
                   'git+' || git_url || (
                       CASE
                         WHEN lm_name ISNULL THEN ''
                         ELSE ('?lm=' || lm_namespace || '/' || lm_name)
                       END
                   ) || '#' || git_ref,
                   repo_transform
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

std::string transforms_to_json(const std::vector<fs_transformation>& trs) {
    std::string acc = "[";
    for (auto it = trs.begin(); it != trs.end(); ++it) {
        acc += it->as_json();
        if (std::next(it) != trs.end()) {
            acc += ", ";
        }
    }
    return acc + "]";
}

void store_with_remote(const neo::sqlite3::statement_cache&,
                       const package_info& pkg,
                       std::monostate) {
    neo_assert_always(
        invariant,
        false,
        "There was an attempt to insert a package listing into the database where that package "
        "listing does not have a remote listing. If you see this message, it is a dds bug.",
        pkg.ident.to_string());
}

void store_with_remote(neo::sqlite3::statement_cache& stmts,
                       const package_info&            pkg,
                       const http_remote_listing&     http) {
    nsql::exec(  //
        stmts(R"(
            INSERT OR REPLACE INTO dds_cat_pkgs (
                name,
                version,
                remote_url,
                description,
                repo_transform
            ) VALUES (?1, ?2, ?3, ?4, ?5)
        )"_sql),
        pkg.ident.name,
        pkg.ident.version.to_string(),
        http.url,
        pkg.description,
        transforms_to_json(http.transforms));
}

void store_with_remote(neo::sqlite3::statement_cache& stmts,
                       const package_info&            pkg,
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
                description,
                repo_transform
            ) VALUES (
                ?1,
                ?2,
                ?3,
                ?4,
                ?5
            )
        )"_sql),
        pkg.ident.name,
        pkg.ident.version.to_string(),
        url,
        pkg.description,
        transforms_to_json(git.transforms));
}

void do_store_pkg(neo::sqlite3::database&        db,
                  neo::sqlite3::statement_cache& st_cache,
                  const package_info&            pkg) {
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
        dds_log(critical, "Root of catalog dds_cat_meta cell should be a JSON object");
        throw_external_error<errc::corrupted_catalog_db>();
    }

    auto version_ = meta["version"];
    if (!version_.is_number_integer()) {
        dds_log(critical, "'version' key in dds_cat_meta is not an integer");
        throw_external_error<errc::corrupted_catalog_db>(
            "The catalog database metadata is invalid [bad dds_meta.version]");
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
        dds_log(debug, "Applying catalog migration 1");
        migrate_repodb_1(db);
    }
    if (version < 2) {
        dds_log(debug, "Applying catalog migration 2");
        migrate_repodb_2(db);
    }
    if (version < 3) {
        dds_log(debug, "Applying catalog migration 3");
        migrate_repodb_3(db);
    }
    meta["version"] = current_database_version;
    exec(db.prepare("UPDATE dds_cat_meta SET meta=?"), meta.dump());
}

void check_json(bool b, std::string_view what) {
    if (!b) {
        throw_user_error<errc::invalid_catalog_json>("Catalog JSON is invalid: {}", what);
    }
}

}  // namespace

catalog catalog::open(const std::string& db_path) {
    if (db_path != ":memory:") {
        auto pardir = fs::weakly_canonical(db_path).parent_path();
        dds_log(trace, "Ensuring parent directory [{}]", pardir.string());
        fs::create_directories(pardir);
    }
    dds_log(debug, "Opening package catalog [{}]", db_path);
    auto db = nsql::database::open(db_path);
    try {
        ensure_migrated(db);
    } catch (const nsql::sqlite3_error& e) {
        dds_log(critical,
                "Failed to load the repository database. It appears to be invalid/corrupted. The "
                "exception message is: {}",
                e.what());
        throw_external_error<errc::corrupted_catalog_db>();
    }
    dds_log(trace, "Successfully opened catalog");
    return catalog(std::move(db));
}

catalog::catalog(nsql::database db)
    : _db(std::move(db)) {}

void catalog::store(const package_info& pkg) {
    nsql::transaction_guard tr{_db};
    do_store_pkg(_db, _stmt_cache, pkg);
}

std::optional<package_info> catalog::get(const package_id& pk_id) const noexcept {
    auto ver_str = pk_id.version.to_string();
    dds_log(trace, "Lookup package {}@{}", pk_id.name, ver_str);
    auto& st = _stmt_cache(R"(
        SELECT
            pkg_id,
            name,
            version,
            remote_url,
            description,
            repo_transform
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
                      "Failed to pull a package from the catalog database",
                      ec,
                      pk_id.to_string(),
                      nsql::error_category().message(int(ec)));

    const auto& [pkg_id, name, version, remote_url, description, repo_transform]
        = st.row()
              .unpack<std::int64_t,
                      std::string,
                      std::string,
                      std::string,
                      std::string,
                      std::string>();

    ec = st.step(std::nothrow);
    if (ec == nsql::errc::row) {
        dds_log(warn,
                "There is more than one entry for package {} in the catalog database. One will be "
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

    auto info = package_info{
        pk_id,
        std::move(deps),
        std::move(description),
        parse_remote_url(remote_url),
    };

    if (!repo_transform.empty()) {
        // Transforms are stored in the DB as JSON strings. Convert them back to real objects.
        auto tr_data = json5::parse_data(repo_transform);
        check_json(tr_data.is_array(),
                   fmt::format("Database record for {} has an invalid 'repo_transform' field [1]",
                               pkg_id));
        for (const auto& el : tr_data.as_array()) {
            check_json(
                el.is_object(),
                fmt::format("Database record for {} has an invalid 'repo_transform' field [2]",
                            pkg_id));
            auto tr = fs_transformation::from_json(el);
            std::visit(
                [&](auto& remote) {
                    if constexpr (neo::alike<decltype(remote), std::monostate>) {
                        // Do nothing
                    } else {
                        remote.transforms.push_back(std::move(tr));
                    }
                },
                info.remote);
        }
    }
    return info;
}

auto pair_to_pkg_id = [](auto&& pair) {
    const auto& [name, ver] = pair;
    return package_id{name, semver::version::parse(ver)};
};

std::vector<package_id> catalog::all() const noexcept {
    return nsql::exec_tuples<std::string, std::string>(
               _stmt_cache("SELECT name, version FROM dds_cat_pkgs"_sql))
        | neo::lref                                 //
        | ranges::views::transform(pair_to_pkg_id)  //
        | ranges::to_vector;
}

std::vector<package_id> catalog::by_name(std::string_view sv) const noexcept {
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

std::vector<dependency> catalog::dependencies_of(const package_id& pkg) const noexcept {
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

void catalog::import_json_str(std::string_view content) {
    dds_log(trace, "Importing JSON string into catalog");
    auto pkgs = parse_packages_json(content);

    nsql::transaction_guard tr{_db};
    for (const auto& pkg : pkgs) {
        do_store_pkg(_db, _stmt_cache, pkg);
    }
}
