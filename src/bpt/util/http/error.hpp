#pragma once

#include <stdexcept>
#include <string>

namespace bpt {

struct http_error : std::runtime_error {
private:
    int _resp_code;

public:
    using runtime_error::runtime_error;

    explicit http_error(int status, std::string message)
        : runtime_error(message)
        , _resp_code(status) {}

    int status_code() const noexcept { return _resp_code; }
};

struct http_status_error : http_error {
    using http_error::http_error;
};

struct http_server_error : http_error {
    using http_error::http_error;
};

struct e_http_request_url {
    std::string value;
};

enum class e_http_status : int {};

}  // namespace bpt
