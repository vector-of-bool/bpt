#include "./repo.hpp"

#include "./error.hpp"

#include <bpt/error/handle.hpp>
#include <bpt/error/on_error.hpp>
#include <bpt/error/result.hpp>
#include <bpt/error/try_catch.hpp>
#include <bpt/sdist/dist.hpp>
#include <bpt/temp.hpp>
#include <bpt/util/compress.hpp>
#include <bpt/util/db/migrate.hpp>
#include <bpt/util/db/query.hpp>
#include <bpt/util/fs/io.hpp>
#include <bpt/util/fs/shutil.hpp>
#include <bpt/util/log.hpp>

#include <neo/event.hpp>
#include <neo/memory.hpp>
#include <neo/sqlite3/database.hpp>
#include <neo/sqlite3/error.hpp>
#include <neo/sqlite3/transaction.hpp>
#include <neo/tar/util.hpp>
#include <neo/ufmt.hpp>

using namespace bpt;
using namespace bpt::crs;
using namespace neo::sqlite3::literals;

using path_ref = const std::filesystem::path&;

namespace {

void ensure_migrated(unique_database& db) {
    apply_db_migrations(db, "crs_repo_meta", [](unique_database& db) {
        db.exec_script(R"(
            CREATE TABLE crs_repo_self (
                rowid INTEGER PRIMARY KEY,
                name TEXT NOT NULL
            );

            CREATE TABLE crs_repo_packages (
                package_id INTEGER PRIMARY KEY,
                meta_json TEXT NOT NULL,
                name TEXT NOT NULL
                    GENERATED ALWAYS AS (json_extract(meta_json, '$.name'))
                    VIRTUAL,
                version TEXT NOT NULL
                    GENERATED ALWAYS AS (json_extract(meta_json, '$.version'))
                    VIRTUAL,
                pkg_version INTEGER NOT NULL
                    GENERATED ALWAYS AS (json_extract(meta_json, '$.pkg-version'))
                    VIRTUAL,
                UNIQUE(name, version, pkg_version)
            );
        )"_sql);
    }).value();
}

void copy_source_tree(path_ref from_dir, path_ref to_dir) {
    bpt_log(debug, "Copy source tree [{}] -> [{}]", from_dir.string(), to_dir.string());
    fs::create_directories(to_dir.parent_path());
    bpt::copy_tree(from_dir, to_dir, fs::copy_options::recursive).value();
}

void copy_library(path_ref lib_root, path_ref to_root) {
    auto src_dir = lib_root / "src";
    if (fs::is_directory(src_dir)) {
        copy_source_tree(src_dir, to_root / "src");
    }
    auto inc_dir = lib_root / "include";
    if (fs::is_directory(inc_dir)) {
        copy_source_tree(inc_dir, to_root / "include");
    }
}

void archive_package_libraries(path_ref from_dir, path_ref dest_root, const package_info& pkg) {
    for (auto& lib : pkg.libraries) {
        fs::path relpath = lib.path;
        auto     dest    = dest_root / relpath;
        auto     src     = from_dir / relpath;
        copy_library(src, dest);
    }
}

}  // namespace

void repository::_vacuum_and_compress() const {
    neo_assert(invariant,
               !_db.sqlite3_db().is_transaction_active(),
               "Database cannot be recompressed while a transaction is open");
    db_exec(_prepare("VACUUM"_sql)).value();
    bpt::compress_file_gz(_dirpath / "repo.db", _dirpath / "repo.db.gz").value();
}

repository repository::create(path_ref dirpath, std::string_view name) {
    BPT_E_SCOPE(e_repo_open_path{dirpath});
    std::filesystem::create_directories(dirpath);
    auto db = unique_database::open((dirpath / "repo.db").string()).value();
    ensure_migrated(db);
    bpt_leaf_try {
        db_exec(db.prepare("INSERT INTO crs_repo_self (rowid, name) VALUES (1729, ?)"_sql), name)
            .value();
    }
    bpt_leaf_catch(matchv<neo::sqlite3::errc::constraint_primary_key>) {
        BOOST_LEAF_THROW_EXCEPTION(current_error(), e_repo_already_init{});
    };
    auto r = repository{std::move(db), dirpath};
    r._vacuum_and_compress();
    return r;
}

repository repository::open_existing(path_ref dirpath) {
    BPT_E_SCOPE(e_repo_open_path{dirpath});
    auto db = unique_database::open_existing((dirpath / "repo.db").string()).value();
    ensure_migrated(db);
    return repository{std::move(db), dirpath};
}

neo::sqlite3::statement& repository::_prepare(neo::sqlite3::sql_string_literal sql) const {
    return _db.prepare(sql);
}

std::string repository::name() const {
    return db_cell<std::string>(_prepare("SELECT name FROM crs_repo_self WHERE rowid=1729"_sql))
        .value();
}

fs::path repository::subdir_of(const package_info& pkg) const noexcept {
    return this->pkg_dir() / pkg.id.name.str
        / neo::ufmt("{}~{}", pkg.id.version.to_string(), pkg.id.revision);
}

void repository::import_dir(path_ref dirpath) {
    BPT_E_SCOPE(e_repo_importing_dir{dirpath});
    auto  sd  = sdist::from_directory(dirpath);
    auto& pkg = sd.pkg;
    BPT_E_SCOPE(e_repo_importing_package{pkg});
    auto dest_dir = subdir_of(pkg);
    fs::create_directories(dest_dir);

    // Copy the package into a temporary directory
    auto prep_dir = bpt::temporary_dir::create_in(dest_dir);
    archive_package_libraries(dirpath, prep_dir.path(), pkg);
    fs::create_directories(prep_dir.path());
    bpt::write_file(prep_dir.path() / "pkg.json", pkg.to_json(2));

    auto tmp_tgz = pkg_dir() / (prep_dir.path().filename().string() + ".tgz");
    neo::compress_directory_targz(prep_dir.path(), tmp_tgz);
    neo_defer { std::ignore = ensure_absent(tmp_tgz); };

    if (pkg.id.revision < 1) {
        BOOST_LEAF_THROW_EXCEPTION(e_repo_import_invalid_pkg_version{
            "Package pkg-version must be a positive non-zero integer in order to be imported into "
            "a repository"});
    }

    neo::sqlite3::transaction_guard tr{_db.sqlite3_db()};
    bpt_leaf_try {
        db_exec(  //
            _prepare(R"(
                INSERT INTO crs_repo_packages (meta_json)
                VALUES (?)
            )"_sql),
            std::string_view(pkg.to_json()))
            .value();
    }
    bpt_leaf_catch(matchv<neo::sqlite3::errc::constraint_unique>) {
        BOOST_LEAF_THROW_EXCEPTION(current_error(), e_repo_import_pkg_already_present{});
    };

    move_file(tmp_tgz, dest_dir / "pkg.tgz").value();
    bpt::copy_file(prep_dir.path() / "pkg.json", dest_dir / "pkg.json").value();
    tr.commit();
    _vacuum_and_compress();

    NEO_EMIT(ev_repo_imported_package{*this, dirpath, pkg});
}

neo::any_input_range<package_info> repository::all_packages() const {
    auto& q   = _prepare("SELECT meta_json FROM crs_repo_packages ORDER BY package_id"_sql);
    auto  rst = neo::copy_shared(q.auto_reset());
    return db_query<std::string_view>(q)
        | std::views::transform([pin = rst](auto tup) -> package_info {
               auto [json_str] = tup;
               return package_info::from_json_str(json_str);
           });
}

void repository::remove_pkg(const package_info& meta) {
    auto to_delete = subdir_of(meta);
    db_exec(_prepare(R"(
                DELETE FROM crs_repo_packages
                 WHERE name = ?1
                       AND version = ?2
                       AND (pkg_version = ?3)
            )"_sql),
            meta.id.name.str,
            meta.id.version.to_string(),
            meta.id.revision)
        .value();
    bpt_log(debug, "Deleting subdirectory [{}]", to_delete.string());
    ensure_absent(to_delete).value();
}
