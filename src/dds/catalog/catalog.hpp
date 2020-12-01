#pragma once

#include <dds/deps.hpp>
#include <dds/package/id.hpp>
#include <dds/util/fs.hpp>
#include <dds/util/glob.hpp>

#include "./package_info.hpp"

#include <neo/sqlite3/database.hpp>
#include <neo/sqlite3/statement.hpp>
#include <neo/sqlite3/statement_cache.hpp>
#include <neo/sqlite3/transaction.hpp>

#include <string>
#include <variant>
#include <vector>

namespace dds {

class catalog {
    neo::sqlite3::database                _db;
    mutable neo::sqlite3::statement_cache _stmt_cache{_db};

    explicit catalog(neo::sqlite3::database db);
    catalog(const catalog&) = delete;

public:
    catalog(catalog&&) = default;
    catalog& operator=(catalog&&) = default;

    static catalog open(const std::string& db_path);
    static catalog open(path_ref db_path) { return open(db_path.string()); }

    void                        store(const package_info& info);
    std::optional<package_info> get(const package_id& id) const noexcept;

    std::vector<package_id> all() const noexcept;
    std::vector<package_id> by_name(std::string_view sv) const noexcept;
    std::vector<dependency> dependencies_of(const package_id& pkg) const noexcept;

    void import_json_str(std::string_view json_str);
    void import_json_file(path_ref json_path) {
        auto content = dds::slurp_file(json_path);
        import_json_str(content);
    }

    auto& database() noexcept { return _db; }
    auto& database() const noexcept { return _db; }
};

}  // namespace dds
