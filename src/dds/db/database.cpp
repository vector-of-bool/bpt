#include "./database.hpp"

#include <dds/error/errors.hpp>
#include <dds/util/log.hpp>

#include <neo/sqlite3/exec.hpp>
#include <neo/sqlite3/iter_tuples.hpp>
#include <neo/sqlite3/single.hpp>
#include <neo/sqlite3/transaction.hpp>

#include <nlohmann/json.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

using namespace dds;

namespace nsql = neo::sqlite3;
using nsql::exec;
using namespace nsql::literals;

namespace {

void migrate_1(nsql::database& db) {
    db.exec(R"(
        CREATE TABLE dds_files (
            file_id INTEGER PRIMARY KEY,
            path TEXT NOT NULL UNIQUE
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
        CREATE TABLE dds_deps (
            input_file_id
                INTEGER
                NOT NULL
                REFERENCES dds_files(file_id),
            output_file_id
                INTEGER
                NOT NULL
                REFERENCES dds_files(file_id),
            input_mtime INTEGER NOT NULL,
            UNIQUE(input_file_id, output_file_id)
        );
    )");
}

void ensure_migrated(nsql::database& db) {
    nsql::transaction_guard tr{db};
    db.exec(R"(
        PRAGMA foreign_keys = 1;
        CREATE TABLE IF NOT EXISTS dds_meta AS
            WITH init (meta) AS (VALUES ('{"version": 0}'))
            SELECT * FROM init;
        )");
    auto meta_st     = db.prepare("SELECT meta FROM dds_meta");
    auto [meta_json] = nsql::unpack_single<std::string>(meta_st);

    auto meta = nlohmann::json::parse(meta_json);
    if (!meta.is_object()) {
        throw_external_error<errc::corrupted_build_db>();
    }

    auto version_ = meta["version"];
    if (!version_.is_number_integer()) {
        throw_external_error<errc::corrupted_build_db>(
            "The build database file is corrupted [bad dds_meta.version]");
    }
    int version = version_;
    if (version < 1) {
        migrate_1(db);
    }
    meta["version"] = 1;
    exec(db.prepare("UPDATE dds_meta SET meta=?"), meta.dump());
}

}  // namespace

database database::open(const std::string& db_path) {
    auto db = nsql::database::open(db_path);
    try {
        ensure_migrated(db);
    } catch (const nsql::sqlite3_error& e) {
        dds_log(
            error,
            "Failed to load the databsae. It appears to be invalid/corrupted. We'll delete it and "
            "create a new one. The exception message is: {}",
            e.what());
        fs::remove(db_path);
        db = nsql::database::open(db_path);
        try {
            ensure_migrated(db);
        } catch (const nsql::sqlite3_error& e) {
            dds_log(critical,
                    "Failed to apply database migrations to recovery database. This is a critical "
                    "error. The exception message is: {}",
                    e.what());
            std::terminate();
        }
    }
    return database(std::move(db));
}

database::database(nsql::database db)
    : _db(std::move(db)) {}

std::int64_t database::_record_file(path_ref path_) {
    auto path = fs::weakly_canonical(path_);
    nsql::exec(_stmt_cache(R"(
                    INSERT OR IGNORE INTO dds_files (path)
                    VALUES (?)
                  )"_sql),
               path.generic_string());
    auto& st = _stmt_cache(R"(
        SELECT file_id
          FROM dds_files
         WHERE path = ?1
    )"_sql);
    st.reset();
    auto str         = path.generic_string();
    st.bindings()[1] = str;
    auto [rowid]     = nsql::unpack_single<std::int64_t>(st);
    return rowid;
}

void database::record_dep(path_ref input, path_ref output, fs::file_time_type input_mtime) {
    auto  in_id  = _record_file(input);
    auto  out_id = _record_file(output);
    auto& st     = _stmt_cache(R"(
        INSERT OR REPLACE INTO dds_deps (input_file_id, output_file_id, input_mtime)
        VALUES (?, ?, ?)
    )"_sql);
    nsql::exec(st, in_id, out_id, input_mtime.time_since_epoch().count());
}

void database::store_file_command(path_ref file, const command_info& cmd) {
    auto file_id = _record_file(file);

    auto& st = _stmt_cache(R"(
        INSERT OR REPLACE
          INTO dds_file_commands(file_id, command, output)
        VALUES (?1, ?2, ?3)
    )"_sql);
    nsql::exec(st, file_id, std::string_view(cmd.command), std::string_view(cmd.output));
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
    nsql::exec(st, fs::weakly_canonical(file).generic_string());
}

std::optional<std::vector<input_file_info>> database::inputs_of(path_ref file_) const {
    auto  file = fs::weakly_canonical(file_);
    auto& st   = _stmt_cache(R"(
        WITH file AS (
            SELECT file_id
              FROM dds_files
             WHERE path = ?
        )
        SELECT path, input_mtime
          FROM dds_deps
          JOIN dds_files ON input_file_id = file_id
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

std::optional<command_info> database::command_of(path_ref file_) const {
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
    st.bindings()[1] = file.generic_string();
    auto opt_res     = nsql::unpack_single_opt<std::string, std::string>(st);
    if (!opt_res) {
        return std::nullopt;
    }
    auto& [cmd, out] = *opt_res;
    return command_info{cmd, out};
}