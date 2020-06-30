#pragma once

#include <dds/catalog/git.hpp>
#include <dds/deps.hpp>
#include <dds/package/id.hpp>
#include <dds/util/fs.hpp>
#include <dds/util/glob.hpp>

#include <neo/sqlite3/database.hpp>
#include <neo/sqlite3/statement.hpp>
#include <neo/sqlite3/statement_cache.hpp>
#include <neo/sqlite3/transaction.hpp>

#include <string>
#include <variant>
#include <vector>

namespace dds {

struct repo_transform {
    struct copy_move {
        fs::path from;
        fs::path to;
        int strip_components = 0;
        std::vector<dds::glob> include;
        std::vector<dds::glob> exclude;
    };

    struct remove {
        fs::path path;

        std::vector<dds::glob> only_matching;
    };

    std::optional<copy_move> copy;
    std::optional<copy_move> move;
    std::optional<remove> remove;
};

struct package_info {
    package_id              ident;
    std::vector<dependency> deps;
    std::string             description;

    std::variant<git_remote_listing> remote;

    std::vector<repo_transform> transforms;
};

class catalog {
    neo::sqlite3::database                _db;
    mutable neo::sqlite3::statement_cache _stmt_cache{_db};

    explicit catalog(neo::sqlite3::database db);
    catalog(const catalog&) = delete;

    void _store_pkg(const package_info&, const git_remote_listing&);

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
};

}  // namespace dds