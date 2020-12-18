#include "./response.hpp"

#include <dds/util/log.hpp>

#include <neo/http/parse/header.hpp>

#include <charconv>

using namespace dds;

std::optional<int> http_response_info::content_length() const noexcept {
    auto cl_str = header_value("Content-Length");
    if (!cl_str) {
        return {};
    }
    int  clen     = 0;
    auto conv_res = std::from_chars(cl_str->data(), cl_str->data() + cl_str->size(), clen);
    if (conv_res.ec != std::errc{}) {
        dds_log(warn,
                "The HTTP server returned a non-integral 'Content-Length' header: '{}'. We'll "
                "pretend that there is no 'Content-Length' on this message.",
                *cl_str);
        return {};
    }
    return clen;
}

std::optional<std::string_view> http_response_info::header_value(std::string_view key) const noexcept {
    auto hdr = headers.find(key);
    if (!hdr) {
        return {};
    }
    return hdr->value;
}
