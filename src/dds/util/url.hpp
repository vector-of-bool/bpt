#pragma once

#include <array>

#include <neo/url/url.hpp>

#include <string_view>

namespace dds {

struct e_url_string {
    std::string value;
};

neo::url parse_url(std::string_view sv);

neo::url guess_url_from_string(std::string_view s);

}  // namespace dds
