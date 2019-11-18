#include "./database.hpp"

#include <neo/sqlite3/exec.hpp>
#include <neo/sqlite3/iter_tuples.hpp>
#include <neo/sqlite3/single.hpp>
#include <neo/sqlite3/transaction.hpp>

#include <nlohmann/json.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

using namespace dds;

namespace sqlite3 = neo::sqlite3;
using sqlite3::exec;
using namespace sqlite3::literals;

namespace {

void migrate_1(sqlite3::database& db) {
    db.exec(R"(
        CREATE TABLE dds_files (
            file_id INTEGER PRIMARY KEY,
            path TEXT NOT NULL UNIQUE,
            mtime INTEGER NOT NULL
        );
        CREATE TABLE dds_deps (
            input_file_id
                INTEGER
                NOT NULL
                REFERENCES dds_files(file_id),
            output_file_id
                INTEGER
                NOT NULL
                REFERENCES dds_files(file_id),
            UNIQUE(input_file_id, output_file_id)
        );
        CREATE TABLE dds_file_commands (
            command_id INTEGER PRIMARY KEY,
            file_id
                INTEGER
                UNIQUE
                NOT NULL
                REFERENCES dds_files(file_id),
            command TEXT NOT NULL,
            output TEXT NOT NULL
        );
    )");
}

void ensure_migrated(sqlite3::database& db) {
    sqlite3::transaction_guard tr{db};
    db.exec(R"(
        PRAGMA foreign_keys = 1;
        CREATE TABLE IF NOT EXISTS dds_meta AS
            WITH init (meta) AS (VALUES ('{"version": 0}'))
            SELECT * FROM init;
        )");
    auto meta_st     = db.prepare("SELECT meta FROM dds_meta");
    auto [meta_json] = sqlite3::unpack_single<std::string>(meta_st);

    auto meta = nlohmann::json::parse(meta_json);
    if (!meta.is_object()) {
        throw std::runtime_error("Correupted database file.");
    }

    auto version_ = meta["version"];
    if (!version_.is_number_integer()) {
        throw std::runtime_error("Corrupted database file [bad dds_meta.version]");
    }
    int version = version_;
    if (version < 1) {
        migrate_1(db);
    }
    meta["version"] = 1;
    exec(db, "UPDATE dds_meta SET meta=?", std::forward_as_tuple(meta.dump()));
}

}  // namespace

database database::open(const std::string& db_path) {
    auto db = sqlite3::database::open(db_path);
    try {
        ensure_migrated(db);
    } catch (const sqlite3::sqlite3_error& e) {
        spdlog::error(
            "Failed to load the databsae. It appears to be invalid/corrupted. We'll delete it and "
            "create a new one. The exception message is: {}",
            e.what());
        fs::remove(db_path);
        db = sqlite3::database::open(db_path);
        try {
            ensure_migrated(db);
        } catch (const sqlite3::sqlite3_error& e) {
            spdlog::critical(
                "Failed to apply database migrations to recovery database. This is a critical "
                "error. The exception message is: {}",
                e.what());
            std::terminate();
        }
    }
    return database(std::move(db));
}

database::database(sqlite3::database db)
    : _db(std::move(db)) {}

std::optional<fs::file_time_type> database::last_mtime_of(path_ref file_) {
    auto& st = _stmt_cache(R"(
        SELECT mtime FROM dds_files WHERE path = ?
    )"_sql);
    st.reset();
    auto path      = fs::weakly_canonical(file_);
    st.bindings[1] = path.string();
    auto maybe_res = sqlite3::unpack_single_opt<std::int64_t>(st);
    if (!maybe_res) {
        return std::nullopt;
    }
    auto [timestamp] = *maybe_res;
    return fs::file_time_type(fs::file_time_type::duration(timestamp));
}

void database::store_mtime(path_ref file, fs::file_time_type time) {
    auto& st = _stmt_cache(R"(
        INSERT INTO dds_files (path, mtime)
        VALUES (?1, ?2)
        ON CONFLICT(path) DO UPDATE SET mtime = ?2
    )"_sql);
    sqlite3::exec(st,
                  std::forward_as_tuple(fs::weakly_canonical(file).string(),
                                        time.time_since_epoch().count()));
}

void database::record_dep(path_ref input, path_ref output) {
    auto& st = _stmt_cache(R"(
        WITH input AS (
            SELECT file_id
              FROM dds_files
             WHERE path = ?1
        ),
        output AS (
            SELECT file_id
              FROM dds_files
             WHERE path = ?2
        )
        INSERT OR IGNORE INTO dds_deps (input_file_id, output_file_id)
        VALUES (
            (SELECT * FROM input),
            (SELECT * FROM output)
        )
    )"_sql);
    sqlite3::exec(st,
                  std::forward_as_tuple(fs::weakly_canonical(input).string(),
                                        fs::weakly_canonical(output).string()));
}

void database::store_file_command(path_ref file, const command_info& cmd) {
    auto& st = _stmt_cache(R"(
        WITH file AS (
            SELECT file_id
            FROM dds_files
            WHERE path = ?1
        )
        INSERT OR REPLACE
          INTO dds_file_commands(file_id, command, output)
        VALUES (
            (SELECT * FROM file),
            ?2,
            ?3
        )
    )"_sql);
    sqlite3::exec(st,
                  std::forward_as_tuple(fs::weakly_canonical(file).string(),
                                        std::string_view(cmd.command),
                                        std::string_view(cmd.output)));
}

void database::forget_inputs_of(path_ref file) {
    auto& st = _stmt_cache(R"(
        WITH id_to_delete AS (
            SELECT file_id
            FROM dds_files
            WHERE path = ?
        )
        DELETE FROM dds_deps
         WHERE output_file_id IN id_to_delete
    )"_sql);
    sqlite3::exec(st, std::forward_as_tuple(fs::weakly_canonical(file).string()));
}

std::optional<std::vector<seen_file_info>> database::inputs_of(path_ref file_) {
    auto  file = fs::weakly_canonical(file_);
    auto& st   = _stmt_cache(R"(
        WITH file AS (
            SELECT file_id
              FROM dds_files
             WHERE path = ?
        ),
        input_ids AS (
            SELECT input_file_id
              FROM dds_deps
             WHERE output_file_id IN file
        )
        SELECT path, mtime
          FROM dds_files
         WHERE file_id IN input_ids
    )"_sql);
    st.reset();
    st.bindings[1] = file.string();
    auto tup_iter  = sqlite3::iter_tuples<std::string, std::int64_t>(st);

    std::vector<seen_file_info> ret;
    for (auto& [path, mtime] : tup_iter) {
        ret.emplace_back(
            seen_file_info{path, fs::file_time_type(fs::file_time_type::duration(mtime))});
    }

    if (ret.empty()) {
        return std::nullopt;
    }
    return ret;
}

std::optional<command_info> database::command_of(path_ref file_) {
    auto  file = fs::weakly_canonical(file_);
    auto& st   = _stmt_cache(R"(
        WITH file AS (
            SELECT file_id
              FROM dds_files
             WHERE path = ?
        )
        SELECT command, output
          FROM dds_file_commands
         WHERE file_id IN file
    )"_sql);
    st.reset();
    st.bindings[1] = file.string();
    auto opt_res   = sqlite3::unpack_single_opt<std::string, std::string>(st);
    if (!opt_res) {
        return std::nullopt;
    }
    auto& [cmd, out] = *opt_res;
    return command_info{cmd, out};
}