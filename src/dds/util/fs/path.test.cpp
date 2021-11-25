#include "./path.hpp"

#include <catch2/catch.hpp>

TEST_CASE("Normalize some paths") {
    auto p = dds::normalize_path("foo/bar");
    CHECK(dds::normalize_path("foo").string() == "foo");
    CHECK(dds::normalize_path("foo/bar").string() == "foo/bar");
    CHECK(dds::normalize_path("foo/bar/").string() == "foo/bar");
    CHECK(dds::normalize_path("foo//bar/").string() == "foo/bar");
    CHECK(dds::normalize_path("foo/./bar/").string() == "foo/bar");
    CHECK(dds::normalize_path("foo/../foo/bar/").string() == "foo/bar");
}
