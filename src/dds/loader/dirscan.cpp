#include "./dirscan.hpp"

#include <dds/util/db/migrate.hpp>
#include <dds/util/db/query.hpp>

using namespace dds;
using namespace neo::sqlite3::literals;

result<file_collector> file_collector::create(unique_database& db) noexcept {
    BOOST_LEAF_CHECK(apply_db_migrations(  //
        db,
        "dds_source_collector_meta",
        [](unique_database& db) {  //
            db.exec_script(R"(
                CREATE TABLE dds_scanned_dirs (
                    dir_id INTEGER PRIMARY KEY,
                    dirpath TEXT NOT NULL UNIQUE
                );
                CREATE TABLE dds_found_files (
                    file_id INTEGER PRIMARY KEY,
                    dir_id INTEGER
                        NOT NULL
                        REFERENCES dds_scanned_dirs
                            ON DELETE CASCADE,
                    relpath TEXT NOT NULL,
                    UNIQUE (dir_id, relpath)
                );
            )"_sql);
        }));
    return file_collector{db};
}

result<std::vector<fs::path>> file_collector::collect(path_ref dirpath) noexcept {
    std::error_code ec;
    // Normalize the path so that different paths pointing to the same dir will hit caches
    auto normpath = fs::weakly_canonical(dirpath, ec);
    // Try and select the row corresponding to this directory
    auto it = db_query<std::int64_t>(  //
                  _db.get().prepare("SELECT dir_id FROM dds_scanned_dirs WHERE dirpath = ?"_sql),
                  normpath.string())
                  .begin();

    std::vector<fs::path> ret;

    if (!it.at_end()) {
        // We have a cache-hit
        auto [dir_id] = *it;
        auto rows     = db_query<std::string>(  //
            _db.get().prepare("SELECT relpath FROM dds_found_files WHERE dir_id = ?"_sql),
            dir_id);
        for (auto&& [relpath] : rows) {
            ret.push_back(dirpath / relpath);
        }
    } else {
        // Nothing for this directory. We need to scan it
        auto [dir_id]
            = *db_query<std::int64_t>(
                   _db.get().prepare(
                       "INSERT INTO dds_scanned_dirs (dirpath) VALUES (?) RETURNING dir_id"_sql),
                   normpath.string())
                   .begin();
        for (fs::path child : fs::recursive_directory_iterator{normpath}) {
            auto relpath = child.lexically_relative(normpath);
            ret.push_back(dirpath / relpath);
            db_exec(_db.get().prepare(
                        "INSERT INTO dds_found_files (dir_id, relpath) VALUES (?, ?)"_sql),
                    dir_id,
                    std::string_view(relpath.string()));
        }
    }

    return ret;
}

bool file_collector::has_cached(path_ref dirpath) noexcept {
    auto normpath = fs::weakly_canonical(dirpath);
    auto has_dir  =  //
        db_cell<int>(_db.get().prepare(
                         "VALUES (EXISTS (SELECT * FROM dds_scanned_dirs WHERE dirpath = ?))"_sql),
                     std::string_view(normpath.string()));
    return has_dir != 0;
}

void file_collector::forget(path_ref dirpath) noexcept {
    auto normpath = fs::weakly_canonical(dirpath);
    db_exec(_db.get().prepare("DELETE FROM dds_scanned_dirs WHERE dirpath = ?"_sql),
            std::string_view(normpath.string()));
}
