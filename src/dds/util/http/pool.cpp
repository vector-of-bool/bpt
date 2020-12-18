#include "./pool.hpp"

#include <dds/error/errors.hpp>
#include <dds/util/result.hpp>

#include <boost/leaf/exception.hpp>
#include <fmt/format.h>
#include <neo/gzip_io.hpp>
#include <neo/http/parse/chunked.hpp>
#include <neo/http/request.hpp>
#include <neo/http/response.hpp>
#include <neo/io/openssl/engine.hpp>
#include <neo/io/stream/buffers.hpp>
#include <neo/io/stream/socket.hpp>

#include <map>

namespace dds::detail {

struct http_client_impl {
    network_origin origin;
    explicit http_client_impl(network_origin o)
        : origin(std::move(o)) {}

    enum class _state_t {
        ready,
        sent_req_head,
        sent_req_body,
        recvd_resp_head,
    };

    _state_t _state = _state_t::ready;

    neo::socket _conn;

    std::string _host_string;

    using sock_buffers = neo::stream_io_buffers<neo::socket&>;
    sock_buffers _sock_in{_conn};

    using ssl_engine  = neo::ssl::engine<sock_buffers&, sock_buffers>;
    using ssl_buffers = neo::stream_io_buffers<ssl_engine>;
    std::optional<ssl_buffers> _ssl_in;

    template <typename Fun>
    auto _do_io(Fun&& fn) {
        if (_ssl_in.has_value()) {
            return fn(*_ssl_in);
        } else {
            return fn(_sock_in);
        }
    }

    void connect() {
        DDS_E_SCOPE(origin);
        auto addr = neo::address::resolve(origin.hostname, std::to_string(origin.port));
        auto sock = neo::socket::open_connected(addr, neo::socket::type::stream);

        _conn = std::move(sock);

        if (origin.protocol == "https") {
            static neo::ssl::openssl_app_init ssl_init;
            static neo::ssl::context ssl_ctx{neo::ssl::protocol::tls_any, neo::ssl::role::client};
            _ssl_in.emplace(ssl_engine{ssl_ctx, _sock_in, neo::stream_io_buffers{_conn}});
            _ssl_in->stream().connect();
        } else if (origin.protocol == "http") {
            // Plain HTTP, nothing special to do
        } else {
            throw_user_error<errc::invalid_remote_url>("Unknown protocol: {}", origin.protocol);
        }
    }

    void send_head(const http_request_params& params) {
        neo_assert(invariant,
                   _state == _state_t::ready,
                   "Invalid state for http_client::send_head()",
                   int(_state),
                   params.method,
                   params.path,
                   params.query,
                   origin.hostname,
                   origin.protocol,
                   origin.port);

        neo::http::request_line start_line{
            .method_view = params.method,
            .target = neo::http::origin_form_target{
                .path_view  = params.path,
                .query_view = params.query,
                .has_query  = !params.query.empty(),
                .parse_tail = {},
            },
            .http_version = neo::http::version::v1_1,
            .parse_tail = {},
        };

        auto content_len_str = std::to_string(params.content_length);
        auto hostname_port   = fmt::format("{}:{}", origin.hostname, origin.port);

        std::pair<std::string_view, std::string_view> headers[] = {
            {"Host", hostname_port},
            {"Accept", "*/*"},
            {"Content-Length", content_len_str},
            {"TE", "gzip, chunked, plain"},
            {"Connection", "keep-alive"},
        };

        _do_io([&](auto&& sink) {
            neo::http::write_request(sink, start_line, headers, neo::const_buffer());
        });
        _state = _state_t::sent_req_head;
        if (params.content_length == 0) {
            _state = _state_t::sent_req_body;
        }
    }

    http_response_info recv_head() {
        neo_assert(invariant,
                   _state == _state_t::sent_req_body,
                   "Invalid state for http_client::recv_head()",
                   int(_state),
                   origin.hostname,
                   origin.protocol,
                   origin.port);
        auto r        = _do_io([&](auto&& source) {
            return neo::http::read_response_head<http_response_info>(source);
        });
        _state        = _state_t::recvd_resp_head;
        auto clen_hdr = r.headers.find(neo::http::standard_headers::content_length);
        if (clen_hdr && clen_hdr->value == "0") {
            _state = _state_t::ready;
        }
        return r;
    }
};

struct origin_order {
    bool operator()(const network_origin& left, const network_origin& right) const noexcept {
        return std::tie(left.protocol, left.hostname, left.port)
            < std::tie(right.protocol, right.hostname, right.port);
    }
};

struct http_pool_impl {
    std::multimap<network_origin, std::shared_ptr<http_client_impl>, origin_order> _clients;
};

}  // namespace dds::detail

using namespace dds;

http_pool::~http_pool() = default;

http_pool::http_pool()
    : _impl(new detail::http_pool_impl) {}

http_client::~http_client() {
    // When the http_client is dropped, return its impl back to the connection pool for this origin
    auto pool = _pool.lock();
    if (pool && _impl) {
        pool->_clients.emplace(_impl->origin, _impl);
    }
}

network_origin network_origin::for_url(neo::url_view url) noexcept {
    auto proto = url.scheme;
    auto host  = url.host.value_or("");
    auto port  = url.port.value_or(proto == "https" ? 443 : 80);
    return {std::string(proto), std::string(host), port};
}

network_origin network_origin::for_url(neo::url const& url) noexcept {
    auto proto = url.scheme;
    auto host  = url.host.value_or("");
    auto port  = url.port.value_or(proto == "https" ? 443 : 80);
    return {std::string(proto), std::string(host), port};
}

http_client http_pool::client_for_origin(const network_origin& origin) {
    auto        iter = _impl->_clients.find(origin);
    http_client ret;
    ret._pool = _impl;
    if (iter == _impl->_clients.end()) {
        // Nothing for this origin yet
        auto ptr = std::make_shared<detail::http_client_impl>(origin);
        ptr->connect();
        ret._impl = ptr;
    } else {
        ret._impl = iter->second;
        _impl->_clients.erase(iter);
    }
    return ret;
}

void http_client::send_head(const http_request_params& params) { _impl->send_head(params); }
http_response_info http_client::recv_head() { return _impl->recv_head(); }

void http_client::_send_buf(neo::const_buffer cbuf) {
    _impl->_do_io([&](auto&& sink) { buffer_copy(sink, cbuf); });
}

namespace {

struct recv_none_state : erased_message_body {
    neo::const_buffer next(std::size_t) override { return {}; }
    void              consume(std::size_t) override {}
};

template <typename Stream>
struct recv_chunked_state : erased_message_body {
    Stream&                             _strm;
    neo::http::chunked_buffers<Stream&> _chunked{_strm};

    explicit recv_chunked_state(Stream& s)
        : _strm(s) {}

    neo::const_buffer next(std::size_t n) override { return _chunked.next(n); }
    void              consume(std::size_t n) override { _chunked.consume(n); }
};

template <typename Stream>
struct recv_gzip_state : erased_message_body {
    Stream&                   _strm;
    neo::gzip_source<Stream&> _gzip{_strm};

    explicit recv_gzip_state(Stream& s)
        : _strm(s) {}

    neo::const_buffer next(std::size_t n) override { return _gzip.next(n); }
    void              consume(std::size_t n) override { _gzip.consume(n); }
};

template <typename Stream>
struct recv_plain_state : erased_message_body {
    Stream&         _strm;
    std::size_t     _size;
    client_impl_ptr _client;

    explicit recv_plain_state(Stream& s, std::size_t size)
        : _strm(s)
        , _size(size) {}

    neo::const_buffer next(std::size_t n) override { return _strm.next((std::min)(n, _size)); }
    void              consume(std::size_t n) override {
        _size -= n;
        return _strm.consume(n);
    }
};

}  // namespace

std::unique_ptr<erased_message_body> http_client::_make_body_reader(const http_response_info& res) {
    neo_assert(
        expects,
        _impl->_state == detail::http_client_impl::_state_t::recvd_resp_head,
        "Invalid state to ready HTTP response body. Have not yet received the response header",
        int(_impl->_state),
        _impl->origin.protocol,
        _impl->origin.hostname,
        _impl->origin.port);
    if (res.status < 200 || res.status == 204 || res.status == 304) {
        return std::make_unique<recv_none_state>();
    }
    return _impl->_do_io([&](auto&& source) -> std::unique_ptr<erased_message_body> {
        using source_type = decltype(source);
        if (res.content_length() == 0) {
            return std::make_unique<recv_none_state>();
        } else if (res.transfer_encoding() == "chunked") {
            return std::make_unique<recv_chunked_state<source_type>>(source);
        } else if (res.transfer_encoding() == "gzip") {
            return std::make_unique<recv_gzip_state<source_type>>(source);
        } else if (!res.transfer_encoding().has_value() && res.content_length() > 0) {
            return std::make_unique<recv_plain_state<source_type>>(source, *res.content_length());
        } else {
            neo_assert(invariant,
                       false,
                       "Unimplemented",
                       res.transfer_encoding().value_or("[null]"));
        }
    });
}

void http_client::discard_body(const http_response_info& resp) {
    auto  reader_ = _make_body_reader(resp);
    auto& reader  = *reader_;
    while (true) {
        auto part = reader.next(1024);
        reader.consume(neo::buffer_size(part));
        if (neo::buffer_is_empty(part)) {
            break;
        }
    }
    _set_ready();
}

void http_client::_set_ready() noexcept {
    _impl->_state = detail::http_client_impl::_state_t::ready;
}

std::pair<http_client, http_response_info>
http_pool::request_with_redirects(std::string_view method, const neo::url& url_) {
    auto url = url_;
    DDS_E_SCOPE(url);
    for (auto i = 0; i <= 100; ++i) {
        auto origin = network_origin::for_url(url);
        auto client = client_for_origin(origin);

        http_request_params params{
            .method = method,
            .path   = url.path,
            .query  = url.query.value_or(""),
        };
        client.send_head(params);
        auto resp = client.recv_head();
        DDS_E_SCOPE(resp);

        if (resp.is_error()) {
            client.discard_body(resp);
            throw boost::leaf::exception(http_status_error("Received an error from HTTP"));
        }

        if (resp.is_redirect()) {
            client.discard_body(resp);
            if (i == 100) {
                throw boost::leaf::exception(
                    http_server_error("Encountered over 100 HTTP redirects. Request aborted."));
            }
            auto loc = resp.headers.find("Location");
            if (!loc) {
                throw boost::leaf::exception(
                    http_server_error("Server sent an invalid response of a 30x redirect without a "
                                      "'Location' header"));
            }
            url = neo::url::parse(loc->value);
            continue;
        }

        return {std::move(client), std::move(resp)};
    }
    neo::unreachable();
}
