#include "./database.hpp"

#include <dds/error/errors.hpp>
#include <dds/util/fs/path.hpp>
#include <dds/util/log.hpp>

#include <neo/sqlite3/error.hpp>
#include <neo/sqlite3/exec.hpp>
#include <neo/sqlite3/iter_tuples.hpp>
#include <neo/sqlite3/statement.hpp>
#include <neo/sqlite3/transaction.hpp>

#include <nlohmann/json.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

using namespace dds;

namespace nsql = neo::sqlite3;
using nsql::exec;
using namespace nsql::literals;
using namespace std::literals;

namespace {

void migrate_1(nsql::connection& db) {
    db.exec(R"(
        DROP TABLE IF EXISTS dds_deps;
        DROP TABLE IF EXISTS dds_file_commands;
        DROP TABLE IF EXISTS dds_files;
        DROP TABLE IF EXISTS dds_compile_deps;
        DROP TABLE IF EXISTS dds_compilations;
        DROP TABLE IF EXISTS dds_source_files;
        CREATE TABLE dds_source_files (
            file_id INTEGER PRIMARY KEY,
            path TEXT NOT NULL UNIQUE
        );
        CREATE TABLE dds_compilations (
            compile_id INTEGER PRIMARY KEY,
            file_id
                INTEGER NOT NULL
                UNIQUE REFERENCES dds_source_files(file_id),
            command TEXT NOT NULL,
            output TEXT NOT NULL,
            toolchain_hash INTEGER NOT NULL,
            n_compilations INTEGER NOT NULL DEFAULT 0,
            avg_duration INTEGER NOT NULL DEFAULT 0
        );
        CREATE TABLE dds_compile_deps (
            input_file_id
                INTEGER NOT NULL
                REFERENCES dds_source_files(file_id),
            output_file_id
                INTEGER NOT NULL
                REFERENCES dds_source_files(file_id),
            input_mtime INTEGER NOT NULL,
            UNIQUE(input_file_id, output_file_id)
        );
    )")
        .throw_if_error();
}

void ensure_migrated(nsql::connection& db) {
    db.exec(R"(
        PRAGMA foreign_keys = 1;
        DROP TABLE IF EXISTS dds_meta;
        CREATE TABLE IF NOT EXISTS dds_meta_1 AS
            WITH init (version) AS (VALUES (''))
            SELECT * FROM init;
        )")
        .throw_if_error();
    nsql::transaction_guard tr{db};

    auto version_st  = *db.prepare("SELECT version FROM dds_meta_1");
    auto version_str = *nsql::one_cell<std::string>(version_st);

    const auto cur_version = "alpha-5-dev1"sv;
    if (cur_version != version_str) {
        if (!version_str.empty()) {
            dds_log(info, "NOTE: A prior version of the project build database was found.");
            dds_log(info, "This is not an error, but incremental builds will be invalidated.");
            dds_log(info, "The database is being upgraded, and no further action is necessary.");
        }
        migrate_1(db);
    }
    nsql::exec(*db.prepare("UPDATE dds_meta_1 SET version=?"), cur_version).throw_if_error();
}

}  // namespace

database database::open(const std::string& db_path) {
    auto db = *nsql::connection::open(db_path);
    try {
        ensure_migrated(db);
    } catch (const nsql::error& e) {
        dds_log(
            error,
            "Failed to load the databsae. It appears to be invalid/corrupted. We'll delete it and "
            "create a new one. The exception message is: {}",
            e.what());
        fs::remove(db_path);
        db = *nsql::connection::open(db_path);
        try {
            ensure_migrated(db);
        } catch (const nsql::error& e) {
            dds_log(critical,
                    "Failed to apply database migrations to recovery database. This is a critical "
                    "error. The exception message is: {}",
                    e.what());
            std::terminate();
        }
    }
    return database(std::move(db));
}

database::database(nsql::connection db)
    : _db(std::move(db)) {}

std::int64_t database::_record_file(path_ref path_) {
    auto path = dds::normalize_path(path_);

    auto found = _stored_file_ids_cache.find(path);
    if (found != _stored_file_ids_cache.end()) {
        return found->second;
    }
    auto& st   = _stmt_cache(R"(
        INSERT INTO dds_source_files (path)
        VALUES (?1)
        ON CONFLICT (path) DO UPDATE SET path=path
        RETURNING file_id
    )"_sql);
    auto [fid] = *nsql::one_row<std::int64_t>(st, path.generic_string());
    _stored_file_ids_cache.emplace(path, fid);
    return fid;
}

void database::record_dep(path_ref input, path_ref output, fs::file_time_type input_mtime) {
    auto  in_id  = _record_file(input);
    auto  out_id = _record_file(output);
    auto& st     = _stmt_cache(R"(
        INSERT OR REPLACE INTO dds_compile_deps (input_file_id, output_file_id, input_mtime)
        VALUES (?, ?, ?)
    )"_sql);
    nsql::exec(st, in_id, out_id, input_mtime.time_since_epoch().count()).throw_if_error();
}

void database::record_compilation(path_ref file, const completed_compilation& cmd) {
    auto file_id = _record_file(file);

    auto& st = _stmt_cache(R"(
        INSERT INTO dds_compilations
                (file_id, command, output, n_compilations, toolchain_hash, avg_duration)
            VALUES
                (:file_id, :command, :output, 1, :toolchain_hash, :duration)
        ON CONFLICT(file_id) DO UPDATE SET
            command = :command,
            output = :output,
            toolchain_hash = :toolchain_hash,
            n_compilations = CASE
                WHEN :duration < 500 THEN n_compilations
                ELSE min(10, n_compilations + 1)
            END,
            avg_duration = CASE
                WHEN :duration < 500 THEN avg_duration
                ELSE avg_duration + ((:duration - avg_duration) / min(10, n_compilations + 1))
            END
    )"_sql);
    nsql::exec(st,
               file_id,
               std::string_view(cmd.quoted_command),
               std::string_view(cmd.output),
               cmd.toolchain_hash,
               cmd.duration.count())
        .throw_if_error();
}

void database::forget_inputs_of(path_ref file) {
    auto& st = _stmt_cache(R"(
        WITH id_to_delete AS (
            SELECT file_id
            FROM dds_source_files
            WHERE path = ?
        )
        DELETE FROM dds_compile_deps
         WHERE output_file_id IN id_to_delete
    )"_sql);
    nsql::exec(st, fs::weakly_canonical(file).generic_string()).throw_if_error();
}

std::optional<std::vector<input_file_info>> database::inputs_of(path_ref file_) const {
    auto  file = fs::weakly_canonical(file_);
    auto& st   = _stmt_cache(R"(
        WITH file AS (
            SELECT file_id
              FROM dds_source_files
             WHERE path = ?
        )
        SELECT path, input_mtime
          FROM dds_compile_deps
          JOIN dds_source_files ON input_file_id = file_id
         WHERE output_file_id IN file
    )"_sql);
    st.reset();
    st.bindings()[1] = file.generic_string();
    auto tup_iter    = nsql::iter_tuples<std::string, std::int64_t>(st);

    std::vector<input_file_info> ret;
    for (auto [path, mtime] : tup_iter) {
        ret.emplace_back(
            input_file_info{path, fs::file_time_type(fs::file_time_type::duration(mtime))});
    }

    if (ret.empty()) {
        return std::nullopt;
    }
    return ret;
}

std::optional<completed_compilation> database::command_of(path_ref file_) const {
    auto  file = fs::weakly_canonical(file_);
    auto& st   = _stmt_cache(R"(
        WITH file AS (
            SELECT file_id
              FROM dds_source_files
             WHERE path = ?
        )
        SELECT command, output, avg_duration, toolchain_hash
          FROM dds_compilations
         WHERE file_id IN file
    )"_sql);
    st.reset();
    st.bindings()[1] = file.generic_string();
    auto opt_res     = nsql::next<std::string, std::string, std::int64_t, std::int64_t>(st);
    if (opt_res.errc() == nsql::errc::done) {
        return std::nullopt;
    }
    auto& [cmd, out, dur, tc_id] = *opt_res;
    return completed_compilation{cmd, out, tc_id, std::chrono::milliseconds(dur)};
}
