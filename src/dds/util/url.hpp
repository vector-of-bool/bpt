#pragma once

#include <neo/url/url.hpp>

#include <string_view>

namespace dds {

neo::url guess_url_from_string(std::string_view s);

}  // namespace dds