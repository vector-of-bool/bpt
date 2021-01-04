#pragma once

#include <dds/error/result_fwd.hpp>

#include <semver/version.hpp>

#include <optional>
#include <string_view>
#include <vector>

namespace neo::sqlite3 {

class database_ref;

}  // namespace neo::sqlite3

namespace dds {

struct pkg_group_search_result {
    std::string                  name;
    std::vector<semver::version> versions;
    std::string                  description;
    std::string                  remote_name;
};

struct pkg_search_results {
    std::vector<pkg_group_search_result> found;
};

result<pkg_search_results> pkg_search(neo::sqlite3::database_ref      db,
                                      std::optional<std::string_view> query) noexcept;

}  // namespace dds
