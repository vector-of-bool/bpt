#include "./catalog.hpp"

#include "./import.hpp"

#include <dds/catalog/init_catalog.hpp>
#include <dds/dym.hpp>
#include <dds/error/errors.hpp>
#include <dds/solve/solve.hpp>

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

#include <spdlog/spdlog.h>

using namespace dds;

namespace sqlite3 = neo::sqlite3;
using namespace sqlite3::literals;

namespace {

void migrate_repodb_1(sqlite3::database& db) {
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

void migrate_repodb_2(sqlite3::database& db) {
    db.exec(R"(
        ALTER TABLE dds_cat_pkgs
            ADD COLUMN repo_transform TEXT NOT NULL DEFAULT '[]'
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
                       const git_remote_listing&      git) {
    auto lm_usage = git.auto_lib.value_or(lm::usage{});
    sqlite3::exec(  //
        stmts,
        R"(
            INSERT OR REPLACE INTO dds_cat_pkgs (
                name,
                version,
                git_url,
                git_ref,
                lm_name,
                lm_namespace,
                description,
                repo_transform
            ) VALUES (
                ?1,
                ?2,
                ?3,
                ?4,
                CASE WHEN ?5 = '' THEN NULL ELSE ?5 END,
                CASE WHEN ?6 = '' THEN NULL ELSE ?6 END,
                ?7,
                ?8
            )
        )"_sql,
        std::forward_as_tuple(  //
            pkg.ident.name,
            pkg.ident.version.to_string(),
            git.url,
            git.ref,
            lm_usage.name,
            lm_usage.namespace_,
            pkg.description,
            transforms_to_json(git.transforms)));
}

void do_store_pkg(neo::sqlite3::database&        db,
                  neo::sqlite3::statement_cache& st_cache,
                  const package_info&            pkg) {
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
        sqlite3::exec(new_dep_st,
                      std::forward_as_tuple(db_pkg_id,
                                            dep.name,
                                            iv_1.low.to_string(),
                                            iv_1.high.to_string()));
    }
}

void store_init_packages(sqlite3::database& db, sqlite3::statement_cache& st_cache) {
    for (auto& pkg : init_catalog_packages()) {
        do_store_pkg(db, st_cache, pkg);
    }
}

void ensure_migrated(sqlite3::database& db) {
    sqlite3::transaction_guard tr{db};
    db.exec(R"(
        PRAGMA foreign_keys = 1;
        CREATE TABLE IF NOT EXISTS dds_cat_meta AS
            WITH init(meta) AS (VALUES ('{"version": 0}'))
            SELECT * FROM init;
    )");
    auto meta_st     = db.prepare("SELECT meta FROM dds_cat_meta");
    auto [meta_json] = sqlite3::unpack_single<std::string>(meta_st);

    auto meta = nlohmann::json::parse(meta_json);
    if (!meta.is_object()) {
        throw_external_error<errc::corrupted_catalog_db>();
    }

    auto version_ = meta["version"];
    if (!version_.is_number_integer()) {
        throw_external_error<errc::corrupted_catalog_db>(
            "The catalog database metadata is invalid [bad dds_meta.version]");
    }

    constexpr int current_database_version = 2;

    int version = version_;

    // If this is the first time we're working here, import the initial
    // catalog with some useful tidbits.
    bool import_init_packages = version == 0;

    if (version > current_database_version) {
        throw_external_error<errc::catalog_too_new>();
    }

    if (version < 1) {
        migrate_repodb_1(db);
    }
    if (version < 2) {
        migrate_repodb_2(db);
    }
    meta["version"] = 2;
    exec(db, "UPDATE dds_cat_meta SET meta=?", std::forward_as_tuple(meta.dump()));

    if (import_init_packages) {
        spdlog::info(
            "A new catalog database case been created, and has been populated with some initial "
            "contents.");
        neo::sqlite3::statement_cache stmts{db};
        store_init_packages(db, stmts);
    }
}

void check_json(bool b, std::string_view what) {
    if (!b) {
        throw_user_error<errc::invalid_catalog_json>("Catalog JSON is invalid: {}", what);
    }
}

}  // namespace

catalog catalog::open(const std::string& db_path) {
    if (db_path != ":memory:") {
        fs::create_directories(fs::weakly_canonical(db_path).parent_path());
    }
    auto db = sqlite3::database::open(db_path);
    try {
        ensure_migrated(db);
    } catch (const sqlite3::sqlite3_error& e) {
        spdlog::critical(
            "Failed to load the repository database. It appears to be invalid/corrupted. The "
            "exception message is: {}",
            e.what());
        throw_external_error<errc::corrupted_catalog_db>();
    }
    return catalog(std::move(db));
}

catalog::catalog(sqlite3::database db)
    : _db(std::move(db)) {}

void catalog::store(const package_info& pkg) {
    sqlite3::transaction_guard tr{_db};

    do_store_pkg(_db, _stmt_cache, pkg);
}

std::optional<package_info> catalog::get(const package_id& pk_id) const noexcept {
    auto& st = _stmt_cache(R"(
        SELECT
            pkg_id,
            name,
            version,
            git_url,
            git_ref,
            lm_name,
            lm_namespace,
            description,
            repo_transform
        FROM dds_cat_pkgs
        WHERE name = ? AND version = ?
    )"_sql);
    st.reset();
    st.bindings  = std::forward_as_tuple(pk_id.name, pk_id.version.to_string());
    auto opt_tup = sqlite3::unpack_single_opt<std::int64_t,
                                              std::string,
                                              std::string,
                                              std::optional<std::string>,
                                              std::optional<std::string>,
                                              std::optional<std::string>,
                                              std::optional<std::string>,
                                              std::string,
                                              std::string>(st);
    if (!opt_tup) {
        dym_target::fill([&] {
            auto all_ids = this->all();
            auto id_strings
                = ranges::views::transform(all_ids, [&](auto id) { return id.to_string(); });
            return did_you_mean(pk_id.to_string(), id_strings);
        });
        return std::nullopt;
    }
    const auto& [pkg_id,
                 name,
                 version,
                 git_url,
                 git_ref,
                 lm_name,
                 lm_namespace,
                 description,
                 repo_transform]
        = *opt_tup;
    assert(pk_id.name == name);
    assert(pk_id.version == semver::version::parse(version));
    assert(git_url);
    assert(git_ref);

    auto deps = dependencies_of(pk_id);

    auto info = package_info{
        pk_id,
        std::move(deps),
        std::move(description),
        git_remote_listing{
            *git_url,
            *git_ref,
            lm_name ? std::make_optional(lm::usage{*lm_namespace, *lm_name}) : std::nullopt,
            {},
        },
    };

    auto append_transform = [](auto transform) {
        return [transform = std::move(transform)](auto& remote) {
            if constexpr (neo::alike<decltype(remote), std::monostate>) {
                // Do nothing
            } else {
                remote.transforms.push_back(std::move(transform));
            }
        };
    };

    if (!repo_transform.empty()) {
        auto tr_json = json5::parse_data(repo_transform);
        check_json(tr_json.is_array(),
                   fmt::format("Database record for {} has an invalid 'repo_transform' field [1]",
                               pkg_id));
        for (const auto& el : tr_json.as_array()) {
            check_json(
                el.is_object(),
                fmt::format("Database record for {} has an invalid 'repo_transform' field [2]",
                            pkg_id));
            auto tr = fs_transformation::from_json(el);
            std::visit(append_transform(tr), info.remote);
        }
    }
    return info;
}

auto pair_to_pkg_id = [](auto&& pair) {
    const auto& [name, ver] = pair;
    return package_id{name, semver::version::parse(ver)};
};

std::vector<package_id> catalog::all() const noexcept {
    return sqlite3::exec_iter<std::string, std::string>(  //
               _stmt_cache,
               "SELECT name, version FROM dds_cat_pkgs"_sql)
        | ranges::views::transform(pair_to_pkg_id)  //
        | ranges::to_vector;
}

std::vector<package_id> catalog::by_name(std::string_view sv) const noexcept {
    return sqlite3::exec_iter<std::string, std::string>(  //
               _stmt_cache,
               R"(
                SELECT name, version
                  FROM dds_cat_pkgs
                 WHERE name = ?
                )"_sql,
               std::tie(sv))                        //
        | ranges::views::transform(pair_to_pkg_id)  //
        | ranges::to_vector;
}

std::vector<dependency> catalog::dependencies_of(const package_id& pkg) const noexcept {
    return sqlite3::exec_iter<std::string,
                              std::string,
                              std::string>(  //
               _stmt_cache,
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
                )"_sql,
               std::forward_as_tuple(pkg.name, pkg.version.to_string()))  //
        | ranges::views::transform([](auto&& pair) {
               auto& [name, low, high] = pair;
               return dependency{name, {semver::version::parse(low), semver::version::parse(high)}};
           })  //
        | ranges::to_vector;
}

void catalog::import_json_str(std::string_view content) {
    auto pkgs = parse_packages_json(content);

    sqlite3::transaction_guard tr{_db};
    for (const auto& pkg : pkgs) {
        store(pkg);
    }
}
