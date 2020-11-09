#pragma once

#include "./base.hpp"

#include <string>
#include <string_view>

namespace dds {

struct git_remote_listing : remote_listing_base {
    std::string url;
    std::string ref;

    void pull_source(path_ref path) const;

    static git_remote_listing from_url(std::string_view sv);
};

}  // namespace dds
