#include <dds/http/session.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Create an HTTP session") {
    auto sess = dds::http_session::connect("google.com", 80);
    auto resp = sess.request_get("/");
}
