#pragma once

#include "./request.hpp"
#include "./response.hpp"

#include <neo/buffer_algorithm/copy.hpp>
#include <neo/buffer_sink.hpp>
#include <neo/buffer_source.hpp>
#include <neo/url.hpp>
#include <neo/url/view.hpp>
#include <neo/utility.hpp>

#include <memory>

namespace dds {

namespace detail {

struct http_pool_access_impl;
struct http_pool_impl;

struct http_client_impl;

}  // namespace detail

struct erased_message_body {
    virtual ~erased_message_body()                   = default;
    virtual neo::const_buffer next(std::size_t n)    = 0;
    virtual void              consume(std::size_t n) = 0;
};

class http_status_error : public std::runtime_error {
    using runtime_error::runtime_error;
};

class http_server_error : public std::runtime_error {
    using runtime_error::runtime_error;
};

struct network_origin {
    std::string protocol;
    std::string hostname;
    int         port = 0;

    static network_origin for_url(neo::url_view url) noexcept;
    static network_origin for_url(const neo::url& url) noexcept;
};

class http_client {
    friend class http_pool;

    std::weak_ptr<detail::http_pool_impl>     _pool;
    std::shared_ptr<detail::http_client_impl> _impl;

    http_client() = default;

    void _send_buf(neo::const_buffer);

    std::unique_ptr<erased_message_body> _make_body_reader(const http_response_info&);
    void                                 _set_ready() noexcept;

public:
    http_client(http_client&& o)
        : _pool(neo::take(o._pool))
        , _impl(neo::take(o._impl)) {}
    ~http_client();

    void send_head(http_request_params const& params);

    http_response_info recv_head();

    template <neo::buffer_input Body>
    void send_body(Body&& body) {
        if constexpr (neo::single_buffer<Body>) {
            _send_buf(body);
        } else if constexpr (neo::buffer_range<Body>) {
            neo::buffers_consumer cons{body};
            send_body(cons);
        } else {
            while (true) {
                auto part = body.next(1024);
                if (neo::buffer_is_empty(part)) {
                    break;
                }
                send_body(part);
                body.consume(neo::buffer_size(part));
            }
        }
    }

    template <neo::buffer_output Out>
    void recv_body_into(const http_response_info& resp, Out&& out) {
        auto&& sink  = neo::ensure_buffer_sink(out);
        auto   state = _make_body_reader(resp);
        neo::buffer_copy(sink, *state);
        _set_ready();
    }

    void discard_body(const http_response_info&);
};

struct request_result {
    http_client        client;
    http_response_info resp;

    void discard_body() { client.discard_body(resp); }
};

class http_pool {
    friend class http_client;
    std::shared_ptr<detail::http_pool_impl> _impl;

public:
    http_pool();
    http_pool(http_pool&&) = default;
    http_pool& operator=(http_pool&&) = default;
    ~http_pool();

    static http_pool& thread_local_pool() {
        thread_local http_pool inst;
        return inst;
    }

    static http_pool& global_pool() {
        static http_pool inst;
        return inst;
    }

    http_client client_for_origin(const network_origin&);

    request_result request(neo::url url, http_request_params params);
    auto           request(neo::url url) { return request(url, http_request_params{}); }
};

}  // namespace dds
