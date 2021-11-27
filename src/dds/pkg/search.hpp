#pragma once

#include <dds/error/result_fwd.hpp>

#include <neo/sqlite3/fwd.hpp>
#include <semver/version.hpp>

#include <optional>
#include <string_view>
#include <vector>

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

result<pkg_search_results> pkg_search(neo::sqlite3::connection_ref    db,
                                      std::optional<std::string_view> query) noexcept;

}  // namespace dds
