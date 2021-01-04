#pragma once

#include <neo/http/headers.hpp>
#include <neo/http/version.hpp>

#include <string>

namespace dds {

struct http_response_info {
    int                status;
    std::string        status_message;
    neo::http::version version;
    neo::http::headers headers;

    std::size_t head_byte_size = 0;

    void throw_for_status() const;

    bool is_client_error() const noexcept { return status >= 400 && status < 500; }
    bool is_server_error() const noexcept { return status >= 500 && status < 600; }
    bool is_error() const noexcept { return is_client_error() || is_server_error(); }
    bool is_redirect() const noexcept { return status >= 300 && status < 400; }
    bool not_modified() const noexcept { return status == 304; }

    std::optional<std::string_view> header_value(std::string_view key) const noexcept;
    std::optional<int>              content_length() const noexcept;

    auto location() const noexcept { return header_value("Location"); }
    auto transfer_encoding() const noexcept { return header_value("Transfer-Encoding"); }
    auto etag() const noexcept { return header_value("ETag"); }
    auto last_modified() const noexcept { return header_value("Last-Modified"); }
};

}  // namespace dds
