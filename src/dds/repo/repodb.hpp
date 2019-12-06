#pragma once

#include <dds/deps.hpp>
#include <dds/package_id.hpp>
#include <dds/repo/remote.hpp>
#include <dds/util/fs.hpp>

#include <neo/sqlite3/database.hpp>
#include <neo/sqlite3/statement.hpp>
#include <neo/sqlite3/statement_cache.hpp>
#include <neo/sqlite3/transaction.hpp>

#include <vector>

namespace dds {

struct package_info {
    package_id              ident;
    std::vector<dependency> deps;

    std::variant<git_remote_listing> remote;
};

class repo_database {
    neo::sqlite3::database                _db;
    mutable neo::sqlite3::statement_cache _stmt_cache{_db};

    explicit repo_database(neo::sqlite3::database db);
    repo_database(const repo_database&) = delete;

    void _store_pkg(const package_info&, const git_remote_listing&);

public:
    repo_database(repo_database&&) = default;
    repo_database& operator=(repo_database&&) = default;

    static repo_database open(const std::string& db_path);
    static repo_database open(path_ref db_path) { return open(db_path.string()); }

    void store(const package_info& info);

    std::vector<package_id> by_name(std::string_view sv) const noexcept;
    std::vector<dependency> dependencies_of(const package_id& pkg) const noexcept;

    void import_json_str(std::string_view json_str);
    void import_json_file(path_ref json_path) {
        auto content = dds::slurp_file(json_path);
        import_json_str(content);
    }
};

}  // namespace dds