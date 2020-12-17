#include "./repoman.hpp"

#include <dds/package/manifest.hpp>
#include <dds/util/log.hpp>
#include <dds/util/result.hpp>

#include <neo/gzip.hpp>
#include <neo/inflate.hpp>
#include <neo/io/stream/buffers.hpp>
#include <neo/io/stream/file.hpp>
#include <neo/sqlite3/exec.hpp>
#include <neo/sqlite3/single.hpp>
#include <neo/sqlite3/transaction.hpp>
#include <neo/tar/ustar.hpp>
#include <neo/transform_io.hpp>
#include <neo/utility.hpp>
#include <nlohmann/json.hpp>

using namespace dds;

namespace nsql = neo::sqlite3;
using namespace nsql::literals;

namespace {

void migrate_db_1(nsql::database_ref db) {
    db.exec(R"(
        CREATE TABLE dds_repo_packages (
            package_id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            version TEXT NOT NULL,
            description TEXT NOT NULL,
            url TEXT NOT NULL,
            UNIQUE (name, version)
        );

        CREATE TABLE dds_repo_package_deps (
            dep_id INTEGER PRIMARY KEY,
            package_id INTEGER NOT NULL
                REFERENCES dds_repo_packages
                ON DELETE CASCADE,
            dep_name TEXT NOT NULL,
            low TEXT NOT NULL,
            high TEXT NOT NULL,
            UNIQUE(package_id, dep_name)
        );
    )");
}

void ensure_migrated(nsql::database_ref db, std::optional<std::string_view> name) {
    db.exec(R"(
        PRAGMA busy_timeout = 6000;
        PRAGMA foreign_keys = 1;
        CREATE TABLE IF NOT EXISTS dds_repo_meta (
            meta_version INTEGER DEFAULT 1,
            version INTEGER NOT NULL,
            name TEXT NOT NULL
        );

        -- Insert the initial metadata
        INSERT INTO dds_repo_meta (version, name)
            SELECT 0, 'dds-repo-' || lower(hex(randomblob(6)))
            WHERE NOT EXISTS (SELECT 1 FROM dds_repo_meta);
    )");
    nsql::transaction_guard tr{db};

    auto meta_st   = db.prepare("SELECT version FROM dds_repo_meta");
    auto [version] = nsql::unpack_single<int>(meta_st);

    constexpr int current_database_version = 1;
    if (version < 1) {
        migrate_db_1(db);
    }

    nsql::exec(db.prepare("UPDATE dds_repo_meta SET version=?"), current_database_version);
    if (name) {
        nsql::exec(db.prepare("UPDATE dds_repo_meta SET name=?"), *name);
    }
}

}  // namespace

repo_manager repo_manager::create(path_ref directory, std::optional<std::string_view> name) {
    {
        DDS_E_SCOPE(e_init_repo{directory});
        fs::create_directories(directory);
        auto db_path = directory / "repo.db";
        auto db      = nsql::database::open(db_path.string());
        DDS_E_SCOPE(e_init_repo_db{db_path});
        DDS_E_SCOPE(e_open_repo_db{db_path});
        ensure_migrated(db, name);
        fs::create_directories(directory / "pkg");
    }
    return open(directory);
}

repo_manager repo_manager::open(path_ref directory) {
    DDS_E_SCOPE(e_open_repo{directory});
    auto db_path = directory / "repo.db";
    DDS_E_SCOPE(e_open_repo_db{db_path});
    if (!fs::is_regular_file(db_path)) {
        throw std::system_error(make_error_code(std::errc::no_such_file_or_directory),
                                "The database file does not exist");
    }
    auto db = nsql::database::open(db_path.string());
    ensure_migrated(db, std::nullopt);
    return repo_manager{fs::canonical(directory), std::move(db)};
}

std::string repo_manager::name() const noexcept {
    auto [name] = nsql::unpack_single<std::string>(_stmts("SELECT name FROM dds_repo_meta"_sql));
    return name;
}

void repo_manager::import_targz(path_ref tgz_file) {
    neo_assertion_breadcrumbs("Importing targz file", tgz_file.string());
    DDS_E_SCOPE(e_repo_import_targz{tgz_file});
    dds_log(info, "Importing sdist archive [{}]", tgz_file.string());
    neo::ustar_reader tar{
        neo::buffer_transform_source{neo::stream_io_buffers{
                                         neo::file_stream::open(tgz_file, neo::open_mode::read)},
                                     neo::gzip_decompressor{neo::inflate_decompressor{}}}};

    std::optional<package_manifest> man;

    for (auto mem : tar) {
        if (fs::path(mem.filename_str()).lexically_normal()
            == neo::oper::none_of("package.jsonc", "package.json5", "package.json")) {
            continue;
        }

        auto content        = tar.all_data();
        auto synth_filename = tgz_file / mem.filename_str();
        man                 = package_manifest::load_from_json5_str(std::string_view(content),
                                                    synth_filename.string());
        break;
    }

    if (!man) {
        dds_log(critical,
                "Given archive [{}] does not contain a package manifest file",
                tgz_file.string());
        throw std::runtime_error("Invalid package archive");
    }

    DDS_E_SCOPE(man->pkg_id);

    neo::sqlite3::transaction_guard tr{_db};

    dds_log(debug, "Recording package {}@{}", man->pkg_id.name, man->pkg_id.version.to_string());
    nsql::exec(  //
        _stmts(R"(
            INSERT INTO dds_repo_packages (name, version, description, url)
                VALUES (
                    ?1,
                    ?2,
                    'No description',
                    printf('dds:%s@%s', ?1, ?2)
                )
        )"_sql),
        man->pkg_id.name,
        man->pkg_id.version.to_string());

    auto package_id = _db.last_insert_rowid();

    auto& insert_dep_st = _stmts(R"(
        INSERT INTO dds_repo_package_deps(package_id, dep_name, low, high)
            VALUES (?, ?, ?, ?)
    )"_sql);
    for (auto& dep : man->dependencies) {
        assert(dep.versions.num_intervals() == 1);
        auto iv_1 = *dep.versions.iter_intervals().begin();
        dds_log(trace, "  Depends on: {}", dep.to_string());
        nsql::exec(insert_dep_st,
                   package_id,
                   dep.name,
                   iv_1.low.to_string(),
                   iv_1.high.to_string());
    }

    auto dest_path
        = pkg_dir() / man->pkg_id.name / man->pkg_id.version.to_string() / "sdist.tar.gz";
    fs::create_directories(dest_path.parent_path());
    fs::copy(tgz_file, dest_path);

    tr.commit();
}

void repo_manager::delete_package(package_id pkg_id) {
    neo::sqlite3::transaction_guard tr{_db};

    DDS_E_SCOPE(pkg_id);

    nsql::exec(  //
        _stmts(R"(
            DELETE FROM dds_repo_packages
                WHERE name = ?
                  AND version = ?
            )"_sql),
        pkg_id.name,
        pkg_id.version.to_string());
    /// XXX: Verify with _db.changes() that we actually deleted one row

    auto name_dir = pkg_dir() / pkg_id.name;
    auto ver_dir  = name_dir / pkg_id.version.to_string();

    DDS_E_SCOPE(e_repo_delete_path{ver_dir});

    if (!fs::is_directory(ver_dir)) {
        throw std::system_error(std::make_error_code(std::errc::no_such_file_or_directory),
                                "No source archive for the requested package");
    }

    fs::remove_all(ver_dir);

    tr.commit();

    std::error_code ec;
    fs::remove(name_dir, ec);
    if (ec && ec != std::errc::directory_not_empty) {
        throw std::system_error(ec, "Failed to delete package name directory");
    }
}
