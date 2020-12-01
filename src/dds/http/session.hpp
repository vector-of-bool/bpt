#pragma once

#include <neo/http/headers.hpp>
#include <neo/http/version.hpp>
#include <neo/io/openssl/engine.hpp>
#include <neo/io/stream/buffers.hpp>
#include <neo/io/stream/socket.hpp>
#include <neo/string_io.hpp>
#include <neo/url.hpp>

#include <filesystem>
#include <string>

namespace dds {

struct e_http_url {
    std::string value;
};

struct e_http_connect {
    std::string host;
    int         port;
};

struct http_request_params {
    std::string_view method;
    std::string_view path;
    std::string_view query          = "";
    std::size_t      content_length = 0;
};

struct ev_http_request {
    const http_request_params& request;
};

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
};

struct ev_http_response_begin {
    const http_response_info& response;
};

struct ev_http_response_end {
    const http_response_info& response;
};

enum class http_kind {
    plain,
    ssl,
};

class http_session {
    neo::socket _conn;

    std::string _host_string;

    using sock_buffers = neo::stream_io_buffers<neo::socket&>;
    sock_buffers _sock_in{_conn};

    using ssl_engine  = neo::ssl::engine<sock_buffers&, sock_buffers>;
    using ssl_buffers = neo::stream_io_buffers<ssl_engine>;
    std::optional<ssl_buffers> _ssl_in;

    enum _state_t { ready, sent_request, recvd_head } _state = ready;

    template <typename F>
    decltype(auto) _do_io(F&& fn) {
        if (_ssl_in) {
            return fn(*_ssl_in);
        } else {
            return fn(_sock_in);
        }
    }

    void _rebind_refs() {
        if (_ssl_in) {
            _ssl_in->stream().rebind_input(_sock_in);
            _ssl_in->stream().output().rebind_stream(_conn);
        }
    }

public:
    explicit http_session(neo::socket s, std::string host_header)
        : _conn(std::move(s))
        , _host_string(std::move(host_header)) {}

    explicit http_session(neo::socket s, std::string host_header, ssl_engine&& eng)
        : _conn(std::move(s))
        , _host_string(std::move(host_header))
        , _sock_in(_conn, std::move(eng.input().io_buffers()))
        , _ssl_in(std::move(eng)) {
        _rebind_refs();
    }

    http_session(http_session&& other) noexcept
        : _conn(std::move(other._conn))
        , _host_string(std::move(other._host_string))
        , _sock_in(_conn, std::move(other._sock_in.io_buffers()))
        , _ssl_in(std::move(other._ssl_in)) {
        _rebind_refs();
    }

    http_session& operator=(http_session&& other) noexcept {
        _conn                 = std::move(other._conn);
        _host_string          = std::move(other._host_string);
        _sock_in.io_buffers() = std::move(other._sock_in.io_buffers());
        _ssl_in               = std::move(other._ssl_in);
        _rebind_refs();
        return *this;
    }

    void               send_head(http_request_params);
    http_response_info recv_head();
    void recv_body_to_file(http_response_info const& res_head, const std::filesystem::path& dest);

    std::string_view host_string() const noexcept { return _host_string; }

    static http_session connect(const std::string& host, int port);
    static http_session connect_ssl(const std::string& host, int port);

    static http_session connect_for(const neo::url& url);

    std::string request(http_request_params);

    std::string request_get(std::string_view path) {
        return request({.method = "GET", .path = path});
    }

    void download_file(http_request_params, const std::filesystem::path& dest);
};

}  // namespace dds
