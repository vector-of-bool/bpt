#pragma once

#include <neo/sqlite3/database.hpp>
#include <neo/url.hpp>

#include <string_view>

namespace dds {

class pkg_db;

struct e_remote_name {
    std::string value;
};

class pkg_remote {
    std::string _name;
    neo::url    _base_url;

public:
    pkg_remote(std::string name, neo::url url)
        : _name(std::move(name))
        , _base_url(std::move(url)) {}
    pkg_remote() = default;

    static pkg_remote connect(std::string_view url);

    void store(neo::sqlite3::database_ref);
    void update_pkg_db(neo::sqlite3::database_ref,
                       std::optional<std::string_view> etag          = {},
                       std::optional<std::string_view> last_modified = {});
};

void update_all_remotes(neo::sqlite3::database_ref);
void remove_remote(pkg_db& db, std::string_view name);

void add_init_repo(neo::sqlite3::database_ref db) noexcept;

}  // namespace dds
