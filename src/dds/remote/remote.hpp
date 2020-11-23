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

    remote_repository() = default;

public:
    static remote_repository connect(std::string_view url);

    // const repository_manifest& manifest() const noexcept;

    void store(neo::sqlite3::database_ref);
    void update_catalog(neo::sqlite3::database_ref);
};

}  // namespace dds
