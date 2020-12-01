#pragma once

#include <dds/package/id.hpp>
#include <dds/util/fs.hpp>

#include <neo/sqlite3/database.hpp>
#include <neo/sqlite3/iter_tuples.hpp>
#include <neo/sqlite3/statement_cache.hpp>
#include <range/v3/view/transform.hpp>

namespace dds {

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

struct e_repo_delete_targz {
    fs::path path;
};

class repo_manager {
    neo::sqlite3::database                _db;
    mutable neo::sqlite3::statement_cache _stmts{_db};
    fs::path                              _root;

    explicit repo_manager(path_ref root, neo::sqlite3::database db)
        : _db(std::move(db))
        , _root(root) {}

public:
    repo_manager(repo_manager&&) = default;

    static repo_manager create(path_ref directory, std::optional<std::string_view> name);
    static repo_manager open(path_ref directory);

    auto        pkg_dir() const noexcept { return _root / "pkg"; }
    path_ref    root() const noexcept { return _root; }
    std::string name() const noexcept;

    void import_targz(path_ref tgz_path);
    void delete_package(package_id id);

    auto all_packages() const noexcept {
        using namespace neo::sqlite3::literals;
        auto& st   = _stmts("SELECT name, version FROM dds_repo_packages"_sql);
        auto  tups = neo::sqlite3::iter_tuples<std::string, std::string>(st);
        return tups | ranges::views::transform([](auto&& pair) {
                   auto [name, version] = pair;
                   return package_id{name, semver::version::parse(version)};
               });
    }
};

}  // namespace dds
