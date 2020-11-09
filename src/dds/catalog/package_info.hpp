#pragma once

#include "./remote/git.hpp"
#include "./remote/http.hpp"

#include <dds/deps.hpp>
#include <dds/package/id.hpp>
#include <dds/util/fs_transform.hpp>
#include <dds/util/glob.hpp>

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace dds {

using remote_listing_var = std::variant<std::monostate, git_remote_listing, http_remote_listing>;

remote_listing_var parse_remote_url(std::string_view url);

struct package_info {
    package_id              ident;
    std::vector<dependency> deps;
    std::string             description;

    remote_listing_var remote;
};

}  // namespace dds
