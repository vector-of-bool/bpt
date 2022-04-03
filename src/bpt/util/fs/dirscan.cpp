#include "./dirscan.hpp"

#include <bpt/util/db/migrate.hpp>
#include <bpt/util/db/query.hpp>

#include <neo/memory.hpp>
#include <neo/sqlite3/connection_ref.hpp>
#include <neo/sqlite3/exec.hpp>
#include <neo/sqlite3/transaction.hpp>

using namespace dds;
using namespace neo::sqlite3::literals;

file_collector file_collector::create(unique_database& db) {
    apply_db_migrations(  //
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
        })
        .value();
    return file_collector{db};
}

neo::any_input_range<fs::path> file_collector::collect(path_ref dirpath) {
    std::error_code ec;
    // Normalize the path so that different paths pointing to the same dir will hit caches
    auto normpath = fs::weakly_canonical(dirpath, ec);
    // Try and select the row corresponding to this directory
    std::int64_t dir_id  = -1;
    auto         dir_id_ = neo::sqlite3::one_row<std::int64_t>(  //
        _db.get().prepare("SELECT dir_id FROM dds_scanned_dirs WHERE dirpath = ?"_sql),
        normpath.string());

    if (dir_id_.has_value()) {
        dir_id = std::get<0>(*dir_id_);
    } else {
        // Nothing for this directory. We need to scan it
        neo::sqlite3::transaction_guard tr{_db.get().sqlite3_db()};

        auto new_dir_id = *neo::sqlite3::one_row<std::int64_t>(  //
            _db.get().prepare(R"(
                INSERT INTO dds_scanned_dirs (dirpath)
                     VALUES (?)
                  RETURNING dir_id
            )"_sql),
            normpath.string());
        auto dir_iter   = fs::recursive_directory_iterator{normpath};
        auto children   = dir_iter | std::views::transform([&](fs::path p) {
                            return std::tuple(std::get<0>(new_dir_id),
                                              p.lexically_relative(normpath).string());
                        });
        neo::sqlite3::exec_each(  //
            _db.get().prepare(R"(
                INSERT INTO dds_found_files (dir_id, relpath)
                VALUES (?, ?)
            )"_sql),
            children)
            .throw_if_error();
        dir_id = std::get<0>(new_dir_id);
    }

    auto st = neo::copy_shared(
        *_db.get().sqlite3_db().prepare("SELECT relpath FROM dds_found_files WHERE dir_id = ?"));

    return *neo::sqlite3::exec_tuples<std::string>(*st, dir_id)
        | std::views::transform([pin = st](auto tup) { return fs::path(tup.template get<0>()); });
}

bool file_collector::has_cached(path_ref dirpath) noexcept {
    auto normpath = fs::weakly_canonical(dirpath);
    auto has_dir  =  //
        db_cell<bool>(_db.get().prepare(
                          "VALUES (EXISTS (SELECT * FROM dds_scanned_dirs WHERE dirpath = ?))"_sql),
                      std::string_view(normpath.string()));
    return has_dir && *has_dir;
}

void file_collector::forget(path_ref dirpath) noexcept {
    auto normpath = fs::weakly_canonical(dirpath);
    auto res      = db_exec(_db.get().prepare("DELETE FROM dds_scanned_dirs WHERE dirpath = ?"_sql),
                       std::string_view(normpath.string()));
    assert(res);
}
