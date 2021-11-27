#pragma once

#include <dds/pkg/id.hpp>

#include <neo/sqlite3/database.hpp>
#include <neo/sqlite3/statement_cache.hpp>

#include <filesystem>

namespace dds {

namespace fs   = std::filesystem;
using path_ref = const fs::path&;

struct pkg_listing;

struct e_init_repo {
    fs::path path;
};

struct e_open_repo {
    fs::path path;
};

struct e_init_repo_db {
    fs::path path;
};

struct e_open_repo_db {
    fs::path path;
};

struct e_repo_import_targz {
    fs::path path;
};

struct e_repo_delete_path {
    fs::path path;
};

class repo_manager {
    neo::sqlite3::connection              _db;
    mutable neo::sqlite3::statement_cache _stmts{_db};
    fs::path                              _root;

    explicit repo_manager(path_ref root, neo::sqlite3::connection db)
        : _db(std::move(db))
        , _root(root) {}

    void _compress();

public:
    repo_manager(repo_manager&&) = default;

    static repo_manager create(path_ref directory, std::optional<std::string_view> name);
    static repo_manager open(path_ref directory);

    auto        pkg_dir() const noexcept { return _root / "pkg"; }
    path_ref    root() const noexcept { return _root; }
    std::string name() const noexcept;

    void import_targz(path_ref tgz_path);
    void delete_package(pkg_id id);
    void add_pkg(const pkg_listing& info, std::string_view url);

    std::vector<pkg_id> all_packages() const noexcept;
};

}  // namespace dds
