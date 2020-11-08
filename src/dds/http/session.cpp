#include "./session.hpp"

#include <dds/error/errors.hpp>
#include <dds/util/fs.hpp>

#include <fmt/format.h>
#include <fmt/ostream.h>
#include <neo/http/parse/chunked.hpp>
#include <neo/http/request.hpp>
#include <neo/http/response.hpp>
#include <neo/io/openssl/init.hpp>
#include <neo/io/stream/buffers.hpp>
#include <neo/io/stream/file.hpp>
#include <neo/string_io.hpp>

using namespace dds;

namespace {

struct simple_request {
    std::string method;

    std::vector<std::pair<std::string, std::string>> headers;
};

template <typename Out, typename In>
void download_into(Out&& out, In&& in, http_response_info resp) {
    auto resp_te = resp.headers.find(neo::http::standard_headers::transfer_encoding);
    if (resp_te) {
        if (resp_te->value != "chunked") {
            throw std::runtime_error(fmt::format(
                "We can't yet handle HTTP responses that set Transfer-Encoding [Transfer "
                "encoding is '{}']",
                resp_te->value));
        }
        neo::http::chunked_buffers chunked_in{in};
        buffer_copy(out, chunked_in);
    } else {
        auto clen = resp.headers.find(neo::http::standard_headers::content_length);
        neo_assert(invariant, !!clen, "HTTP response has no Content-Length header??");
        buffer_copy(out, in, std::stoull(clen->value));
    }
}

}  // namespace

http_session http_session::connect(const std::string& host, int port) {
    auto addr = neo::address::resolve(host, std::to_string(port));
    auto sock = neo::socket::open_connected(addr, neo::socket::type::stream);

    return http_session{std::move(sock), fmt::format("{}:{}", host, port)};
}

http_session http_session::connect_ssl(const std::string& host, int port) {
    auto addr = neo::address::resolve(host, std::to_string(port));
    auto sock = neo::socket::open_connected(addr, neo::socket::type::stream);

    static neo::ssl::openssl_app_init ssl_init;
    static neo::ssl::context          ssl_ctx{neo::ssl::protocol::tls_any, neo::ssl::role::client};

    neo::stream_io_buffers sock_in{sock};
    ssl_engine             ssl_eng{ssl_ctx, sock_in, neo::stream_io_buffers{sock}};
    ssl_eng.connect();

    return http_session(std::move(sock), fmt::format("{}:{}", host, port), std::move(ssl_eng));
}

void http_session::send_head(http_request_params params) {
    neo_assert_always(invariant,
                      _state == _state_t::ready,
                      "Invalid state for HTTP session to send a request head",
                      _state,
                      params.method,
                      params.path,
                      params.query);
    neo::http::request_line start_line{
        .method_view = params.method,
        .target = neo::http::origin_form_target{
            .path_view = params.path,
            .query_view = params.query,
            .has_query = !params.query.empty(),
            .parse_tail = neo::const_buffer(),
        },
        .http_version = neo::http::version::v1_1,
        .parse_tail = neo::const_buffer(),
    };

    auto cl_str = std::to_string(params.content_length);

    std::pair<std::string_view, std::string_view> headers[] = {
        {"Host", host_string()},
        {"Accept", "*/*"},
        {"Content-Length", cl_str},
    };

    _do_io(
        [&](auto&& io) { neo::http::write_request(io, start_line, headers, neo::const_buffer()); });
    _state = _state_t::sent_request;
}

http_response_info http_session::recv_head() {
    neo_assert_always(invariant,
                      _state == _state_t::sent_request,
                      "Invalid state to receive response head",
                      _state);
    auto r
        = _do_io([&](auto&& io) { return neo::http::read_response_head<http_response_info>(io); });
    _state = _state_t::recvd_head;
    return r;
}

std::string http_session::request(http_request_params params) {
    send_head(params);

    auto resp_head = recv_head();

    neo::string_dynbuf_io resp_body;
    _do_io([&](auto&& io) { download_into(resp_body, io, resp_head); });
    auto body_size = resp_body.available();
    auto str       = std::move(resp_body.string());
    str.resize(body_size);
    return str;
}

void http_session::recv_body_to_file(http_response_info const&    resp_head,
                                     const std::filesystem::path& dest) {
    neo_assert_always(invariant,
                      _state == _state_t::recvd_head,
                      "Invalid state to receive request body",
                      _state,
                      dest);
    auto ofile = neo::file_stream::open(dest, neo::open_mode::write | neo::open_mode::create);
    neo::stream_io_buffers file_out{ofile};
    _do_io([&](auto&& io) { download_into(file_out, io, resp_head); });
    _state = _state_t::ready;
}

void http_session::download_file(http_request_params params, const std::filesystem::path& dest) {
    send_head(params);
    auto resp_head = recv_head();
    if (resp_head.is_error()) {
        throw_external_error<
            errc::http_download_failure>("Failed to download file from {}{} to {}: HTTP {} {}",
                                         host_string(),
                                         params.path,
                                         dest,
                                         resp_head.status,
                                         resp_head.status_message);
    }

    if (resp_head.is_redirect()) {
        throw_external_error<errc::http_download_failure>(
            "dds does not yet support HTTP redirects when downloading data. An HTTP redirect "
            "was encountered when accessing {}{}: It wants to redirect to {}",
            host_string(),
            params.path,
            resp_head.headers["Location"].value);
    }
    recv_body_to_file(resp_head, dest);
}
