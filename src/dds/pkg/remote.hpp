#pragma once

#include <neo/sqlite3/database.hpp>
#include <neo/url.hpp>

#include <string_view>

namespace dds {

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
    void update_pkg_db(neo::sqlite3::database_ref);
};

void update_all_remotes(neo::sqlite3::database_ref);

}  // namespace dds
