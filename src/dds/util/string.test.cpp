#include <dds/util/string.hpp>

#include <dds/util.test.hpp>

using namespace dds;

namespace {

#define CHECK_SPLIT(str, key, ...)                                                                 \
    do {                                                                                           \
        CHECK(dds::split(str, key) == std::vector<std::string>(__VA_ARGS__));                      \
    } while (0)

void test_starts_with() {
    CHECK(starts_with("foo", "foo"));
    CHECK(starts_with("foo.bar", "foo"));
    CHECK(starts_with("foo", "f"));
    CHECK(starts_with("foo", ""));
    CHECK(starts_with("", ""));
    CHECK(!starts_with("foo", "foot"));
    CHECK(!starts_with("", "cat"));
    CHECK(!starts_with("foo.bar", "bar"));
}

void test_ends_with() {
    CHECK(ends_with("foo", "foo"));
    CHECK(ends_with("foo.bar", "bar"));
    CHECK(ends_with("foo", "o"));
    CHECK(ends_with("foo", ""));
    CHECK(ends_with("", ""));
    CHECK(!ends_with("foo", "foot"));
    CHECK(!ends_with("", "cat"));
    CHECK(!ends_with("foo.bar", "foo"));
}

void test_trim() {
    CHECK(trim_view("foo") == "foo");
    CHECK(trim_view("foo   ") == "foo");
    CHECK(trim_view("   ").size() == 0);
}

void test_contains() {
    CHECK(contains("foo", "foo"));
    CHECK(contains("foo", ""));
    CHECK(contains("foo", "o"));
    CHECK(!contains("foo", "bar"));
}

void test_split() {
    CHECK_SPLIT("foo.bar", ".", {"foo", "bar"});
    CHECK_SPLIT("foo.bar.baz", ".", {"foo", "bar", "baz"});
    CHECK_SPLIT(".", ".", {"", ""});
    CHECK_SPLIT("", ",", {""});
}

void run_tests() {
    test_trim();
    test_starts_with();
    test_ends_with();
    test_contains();
    test_split();
}

}  // namespace

DDS_TEST_MAIN;
