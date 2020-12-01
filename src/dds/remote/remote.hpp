#pragma once

#include <dds/util/fs.hpp>
#include <dds/util/result.hpp>

#include <neo/concepts.hpp>
#include <neo/sqlite3/database.hpp>
#include <neo/url.hpp>

#include <string_view>
#include <variant>

namespace dds {

class remote_repository {
    std::string _name;
    neo::url    _base_url;

    remote_repository(std::string name, neo::url url)
        : _name(std::move(name))
        , _base_url(std::move(url)) {}
    remote_repository() = default;

public:
    static remote_repository connect(std::string_view url);

    void store(neo::sqlite3::database_ref);
    void update_catalog(neo::sqlite3::database_ref);
};

void update_all_remotes(neo::sqlite3::database_ref);

}  // namespace dds
