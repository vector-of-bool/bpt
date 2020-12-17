#pragma once

#include "./info.hpp"

#include <dds/util/fs.hpp>

#include <neo/sqlite3/database.hpp>
#include <neo/sqlite3/statement.hpp>
#include <neo/sqlite3/statement_cache.hpp>

#include <string>
#include <vector>

namespace dds {

struct dependency;
struct pkg_id;

class pkg_db {
    neo::sqlite3::database                _db;
    mutable neo::sqlite3::statement_cache _stmt_cache{_db};

    explicit pkg_db(neo::sqlite3::database db);
    pkg_db(const pkg_db&) = delete;

public:
    pkg_db(pkg_db&&) = default;
    pkg_db& operator=(pkg_db&&) = default;

    static pkg_db open(const std::string& db_path);
    static pkg_db open(path_ref db_path) { return open(db_path.string()); }

    static fs::path default_path() noexcept;

    void                    store(const pkg_info& info);
    std::optional<pkg_info> get(const pkg_id& id) const noexcept;

    std::vector<pkg_id>     all() const noexcept;
    std::vector<pkg_id>     by_name(std::string_view sv) const noexcept;
    std::vector<dependency> dependencies_of(const pkg_id& pkg) const noexcept;

    auto& database() noexcept { return _db; }
    auto& database() const noexcept { return _db; }
};

}  // namespace dds
