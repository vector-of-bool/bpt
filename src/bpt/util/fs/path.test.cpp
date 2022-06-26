#include "./path.hpp"

#include <neo/platform.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Normalize some paths") {
    CHECK(bpt::normalize_path("foo").string() == "foo");
    if constexpr (neo::os_is_windows) {
        CHECK(bpt::normalize_path("foo/bar").string() == "foo\\bar");
        CHECK(bpt::normalize_path("foo/bar/").string() == "foo\\bar");
        CHECK(bpt::normalize_path("foo//bar/").string() == "foo\\bar");
        CHECK(bpt::normalize_path("foo/./bar/").string() == "foo\\bar");
        CHECK(bpt::normalize_path("foo/../foo/bar/").string() == "foo\\bar");
    } else {
        CHECK(bpt::normalize_path("foo/bar").string() == "foo/bar");
        CHECK(bpt::normalize_path("foo/bar/").string() == "foo/bar");
        CHECK(bpt::normalize_path("foo//bar/").string() == "foo/bar");
        CHECK(bpt::normalize_path("foo/./bar/").string() == "foo/bar");
        CHECK(bpt::normalize_path("foo/../foo/bar/").string() == "foo/bar");
    }
}
