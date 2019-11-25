#pragma once

#include <dds/util/fs.hpp>

#include <neo/sqlite3/database.hpp>
#include <neo/sqlite3/statement.hpp>
#include <neo/sqlite3/statement_cache.hpp>
#include <neo/sqlite3/transaction.hpp>

#include <chrono>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string_view>

namespace dds {

struct command_info {
    std::string command;
    std::string output;
};

struct input_file_info {
    fs::path           path;
    fs::file_time_type last_mtime;
};

class database {
    neo::sqlite3::database        _db;
    neo::sqlite3::statement_cache _stmt_cache{_db};
    mutable std::shared_mutex     _mutex;

    explicit database(neo::sqlite3::database db);
    database(const database&) = delete;

    std::int64_t _record_file(path_ref p);

public:
    static database open(const std::string& db_path);
    static database open(path_ref db_path) { return open(db_path.string()); }

    auto& mutex() const noexcept { return _mutex; }

    neo::sqlite3::transaction_guard transaction() noexcept {
        return neo::sqlite3::transaction_guard(_db);
    }

    void record_dep(path_ref input, path_ref output, fs::file_time_type input_mtime);
    void store_file_command(path_ref file, const command_info& cmd);
    void forget_inputs_of(path_ref file);

    std::optional<std::vector<input_file_info>> inputs_of(path_ref file);
    std::optional<command_info>                command_of(path_ref file);
};

}  // namespace dds