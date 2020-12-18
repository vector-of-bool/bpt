#include "./pool.hpp"

#include <neo/string_io.hpp>
#include <neo/url.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Create an empty pool") { dds::http_pool pool; }

TEST_CASE("Connect to a remote") {
    dds::http_pool pool;
    // auto           client = pool.access();
    auto cl = pool.client_for_origin({"https", "www.google.com", 443});
    cl.send_head({.method = "GET", .path = "/"});
    // cl.send_head({.method = "GET", .path = "/"});
    auto resp = cl.recv_head();
    CHECK(resp.status == 200);
    CHECK(resp.status_message == "OK");
}

TEST_CASE("Issue a request on a pool") {
    dds::http_pool        pool;
    neo::string_dynbuf_io body;
    auto                  resp = pool.request(neo::url_view::split("https://www.google.com"), body);
    CHECK(resp.status == 200);
    CHECK(body.read_area_view().size() > 5);
}
