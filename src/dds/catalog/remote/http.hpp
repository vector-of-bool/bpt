#pragma once

#include "./base.hpp"

#include <string>
#include <string_view>

namespace dds {

struct http_remote_listing : remote_listing_base {
    std::string url;
    unsigned    strip_components = 0;

    void pull_source(path_ref path) const;

    static http_remote_listing from_url(std::string_view sv);
};

}  // namespace dds
