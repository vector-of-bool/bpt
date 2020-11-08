#pragma once

#include <dds/package/id.hpp>
#include <dds/util/fs.hpp>

#include <libman/package.hpp>

#include <string>
#include <string_view>

namespace dds {

struct http_remote_listing {
    std::string              url;
    unsigned                 strip_components = 0;
    std::optional<lm::usage> auto_lib{};

    void pull_to(path_ref path) const;

    static http_remote_listing from_url(std::string_view sv);
};

}  // namespace dds
