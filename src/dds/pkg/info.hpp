#pragma once

#include "./get/git.hpp"
#include "./get/http.hpp"

#include <dds/deps.hpp>
#include <dds/pkg/id.hpp>
#include <dds/util/glob.hpp>

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace dds {

using remote_listing_var = std::variant<std::monostate, git_remote_listing, http_remote_listing>;

remote_listing_var parse_remote_url(std::string_view url);

struct pkg_info {
    pkg_id                  ident;
    std::vector<dependency> deps;
    std::string             description;

    remote_listing_var remote;
};

}  // namespace dds
