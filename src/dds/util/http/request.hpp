#pragma once

#include <string_view>

#include <neo/http/headers.hpp>

namespace dds {

struct http_request_params {
    std::string_view   method;
    std::string_view   path;
    std::string_view   query          = "";
    std::size_t        content_length = 0;
    neo::http::headers headers{};
};

}  // namespace dds
