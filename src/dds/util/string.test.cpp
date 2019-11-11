#include <dds/util/string.hpp>

#include <catch2/catch.hpp>

using namespace dds;

#define CHECK_SPLIT(str, key, ...)                                                                 \
    do {                                                                                           \
        CHECK(dds::split(str, key) == std::vector<std::string>(__VA_ARGS__));                      \
    } while (0)

TEST_CASE("starts_with") {
    CHECK(starts_with("foo", "foo"));
    CHECK(starts_with("foo.bar", "foo"));
    CHECK(starts_with("foo", "f"));
    CHECK(starts_with("foo", ""));
    CHECK(starts_with("", ""));
    CHECK(!starts_with("foo", "foot"));
    CHECK(!starts_with("", "cat"));
    CHECK(!starts_with("foo.bar", "bar"));
}

TEST_CASE("ends_with") {
    CHECK(ends_with("foo", "foo"));
    CHECK(ends_with("foo.bar", "bar"));
    CHECK(ends_with("foo", "o"));
    CHECK(ends_with("foo", ""));
    CHECK(ends_with("", ""));
    CHECK(!ends_with("foo", "foot"));
    CHECK(!ends_with("", "cat"));
    CHECK(!ends_with("foo.bar", "foo"));
}

TEST_CASE( "trim" )  {
    CHECK(trim_view("foo") == "foo");
    CHECK(trim_view("foo   ") == "foo");
    CHECK(trim_view("   ").size() == 0);
}

TEST_CASE( "contains" )  {
    CHECK(contains("foo", "foo"));
    CHECK(contains("foo", ""));
    CHECK(contains("foo", "o"));
    CHECK(!contains("foo", "bar"));
}

TEST_CASE( "split" )  {
    CHECK_SPLIT("foo.bar", ".", {"foo", "bar"});
    CHECK_SPLIT("foo.bar.baz", ".", {"foo", "bar", "baz"});
    CHECK_SPLIT(".", ".", {"", ""});
    CHECK_SPLIT("", ",", {""});
}
