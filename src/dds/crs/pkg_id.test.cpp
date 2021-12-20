#include "./pkg_id.hpp"

#include <catch2/catch.hpp>

TEST_CASE("Parse a pkg_id string") {
    auto pid = dds::crs::pkg_id::parse_str("foo@1.2.3");
    CHECK(pid.name.str == "foo");
}