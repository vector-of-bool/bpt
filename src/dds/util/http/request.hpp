#pragma once

#include <string_view>

#include <neo/http/headers.hpp>

namespace dds {

struct http_request_params {
    std::string_view method = "GET";
    std::string_view path{};
    std::string_view query{};

    bool follow_redirects = true;

    std::string_view prior_etag{};
    std::string_view last_modified{};
};

}  // namespace dds
