#include "./repoman.hpp"

#include <dds/pkg/listing.hpp>
#include <dds/sdist/package.hpp>
#include <dds/util/compress.hpp>
#include <dds/util/log.hpp>
#include <dds/util/result.hpp>

#include <neo/gzip.hpp>
#include <neo/inflate.hpp>
#include <neo/io/stream/buffers.hpp>
#include <neo/io/stream/file.hpp>
#include <neo/ranges.hpp>
#include <neo/sqlite3/exec.hpp>
#include <neo/sqlite3/statement.hpp>
#include <neo/sqlite3/transaction.hpp>
#include <neo/tar/ustar.hpp>
#include <neo/transform_io.hpp>
#include <neo/utility.hpp>
#include <nlohmann/json.hpp>

#include <fstream>

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
    )")
        .throw_if_error();
}

void migrate_db_2(nsql::database_ref db) {
    db.exec(R"(
        CREATE TABLE dds_repo_deps_1 (
            dep_id INTEGER PRIMARY KEY,
            package_id INTEGER NOT NULL
                REFERENCES dds_repo_packages
                ON DELETE CASCADE,
            dep_name TEXT NOT NULL,
            low TEXT NOT NULL,
            high TEXT NOT NULL,
            kind TEXT NOT NULL,
            UNIQUE(package_id, dep_name, kind)
        );
        INSERT INTO dds_repo_deps_1
            SELECT dep_id, package_id, dep_name, low, high, 'lib'
            FROM dds_repo_package_deps;
        DROP TABLE dds_repo_package_deps;
        ALTER TABLE dds_repo_deps_1 RENAME TO dds_repo_package_deps;
        CREATE INDEX idx_deps_for_pkg ON dds_repo_package_deps (package_id);
    )")
        .throw_if_error();
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
    )")
        .throw_if_error();
    nsql::transaction_guard tr{db};

    auto meta_st   = *db.prepare("SELECT version FROM dds_repo_meta");
    auto [version] = *nsql::unpack_next<int>(meta_st);
    meta_st.reset();

    constexpr int current_database_version = 2;
    if (version < 1) {
        migrate_db_1(db);
    }
    if (version < 2) {
        migrate_db_2(db);
    }

    nsql::exec(*db.prepare("UPDATE dds_repo_meta SET version=?"), current_database_version)
        .throw_if_error();
    if (name) {
        nsql::exec(*db.prepare("UPDATE dds_repo_meta SET name=?"), *name).throw_if_error();
    }

    tr.commit();
    db.exec("VACUUM").throw_if_error();
}

}  // namespace

repo_manager repo_manager::create(path_ref directory, std::optional<std::string_view> name) {
    {
        DDS_E_SCOPE(e_init_repo{directory});
        fs::create_directories(directory);
        auto db_path = directory / "repo.db";
        auto db      = *nsql::database::open(db_path.string());
        DDS_E_SCOPE(e_init_repo_db{db_path});
        DDS_E_SCOPE(e_open_repo_db{db_path});
        ensure_migrated(db, name);
        fs::create_directories(directory / "pkg");
    }
    auto r = open(directory);
    r._compress();
    return r;
}

repo_manager repo_manager::open(path_ref directory) {
    DDS_E_SCOPE(e_open_repo{directory});
    auto db_path = directory / "repo.db";
    DDS_E_SCOPE(e_open_repo_db{db_path});
    if (!fs::is_regular_file(db_path)) {
        throw std::system_error(make_error_code(std::errc::no_such_file_or_directory),
                                "The database file does not exist");
    }
    auto db = *nsql::database::open(db_path.string());
    ensure_migrated(db, std::nullopt);
    return repo_manager{fs::canonical(directory), std::move(db)};
}

std::string repo_manager::name() const noexcept {
    auto [name] = *nsql::unpack_next<std::string>(_stmts("SELECT name FROM dds_repo_meta"_sql));
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

    DDS_E_SCOPE(man->id);

    neo::sqlite3::transaction_guard tr{_db};
    dds_log(debug, "Recording package {}", man->id.to_string());
    dds::pkg_listing info{.ident       = man->id,
                          .deps        = man->dependencies,
                          .description = "[No description]",
                          .remote_pkg  = {}};
    auto             rel_url = fmt::format("dds:{}", man->id.to_string());
    add_pkg(info, rel_url);

    auto dest_path = pkg_dir() / man->id.name.str / man->id.version.to_string() / "sdist.tar.gz";
    fs::create_directories(dest_path.parent_path());
    fs::copy(tgz_file, dest_path);

    tr.commit();
    _compress();
}

void repo_manager::delete_package(pkg_id pkg_id) {
    neo::sqlite3::transaction_guard tr{_db};

    DDS_E_SCOPE(pkg_id);

    nsql::exec(  //
        _stmts(R"(
            DELETE FROM dds_repo_packages
                WHERE name = ?
                  AND version = ?
            )"_sql),
        pkg_id.name.str,
        neo::lref(pkg_id.version.to_string()))
        .throw_if_error();
    /// XXX: Verify with _db.changes() that we actually deleted one row

    auto name_dir = pkg_dir() / pkg_id.name.str;
    auto ver_dir  = name_dir / pkg_id.version.to_string();

    DDS_E_SCOPE(e_repo_delete_path{ver_dir});

    if (!fs::is_directory(ver_dir)) {
        throw std::system_error(std::make_error_code(std::errc::no_such_file_or_directory),
                                "No source archive for the requested package");
    }

    fs::remove_all(ver_dir);

    tr.commit();
    _compress();

    std::error_code ec;
    fs::remove(name_dir, ec);
    if (ec && ec != std::errc::directory_not_empty) {
        throw std::system_error(ec, "Failed to delete package name directory");
    }
}

void repo_manager::add_pkg(const pkg_listing& info, std::string_view url) {
    dds_log(info, "Directly add an entry for {}", info.ident.to_string());
    DDS_E_SCOPE(info.ident);
    nsql::recursive_transaction_guard tr{_db};
    nsql::exec(  //
        _stmts(R"(
            INSERT INTO dds_repo_packages (name, version, description, url)
            VALUES (?, ?, ?, ?)
        )"_sql),
        info.ident.name.str,
        info.ident.version.to_string(),
        info.description,
        url)
        .throw_if_error();

    auto package_rowid = _db.last_insert_rowid();

    auto& insert_dep_st = _stmts(R"(
        INSERT INTO dds_repo_package_deps(package_id, dep_name, low, high, kind)
        VALUES (?, ?, ?, ?, ?)
    )"_sql);

    for (auto& dep : info.deps) {
        assert(dep.versions.num_intervals() == 1);
        auto iv_1 = *dep.versions.iter_intervals().begin();
        dds_log(trace, "  Depends on: {}", dep.decl_to_string());
        nsql::exec(insert_dep_st,
                   package_rowid,
                   dep.name.str,
                   neo::lref(iv_1.low.to_string()),
                   neo::lref(iv_1.high.to_string()),
                   for_kind_str(dep.for_kind))
            .throw_if_error();
    }

    auto dest_dir   = pkg_dir() / info.ident.name.str / info.ident.version.to_string();
    auto stamp_path = dest_dir / "url.txt";
    fs::create_directories(dest_dir);
    std::ofstream stamp_file{stamp_path, std::ios::binary};
    stamp_file << url;

    if (tr.is_top_transaction()) {
        tr.commit();
        _compress();
    }
}

void repo_manager::_compress() {
    _db.exec("VACUUM").throw_if_error();
    dds::compress_file_gz(_root / "repo.db", _root / "repo.db.gz").value();
}

std::vector<pkg_id> repo_manager::all_packages() const noexcept {
    auto& st   = _stmts("SELECT name, version FROM dds_repo_packages"_sql);
    auto  tups = neo::sqlite3::iter_tuples<std::string, std::string>(st);
    return tups | std::views::transform([](auto pair) {
               auto [name, version] = pair;
               return pkg_id{name, semver::version::parse(version)};
           })
        | neo::to_vector;
}