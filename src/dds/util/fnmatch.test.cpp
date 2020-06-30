#include <dds/util/fnmatch.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Basic fnmatch matching") {
    auto pat = dds::fnmatch::compile("foo.bar");
    CHECK_FALSE(pat.match("foo.baz"));
    CHECK_FALSE(pat.match("foo."));
    CHECK_FALSE(pat.match("foo.barz"));
    CHECK_FALSE(pat.match("foo.bar "));
    CHECK_FALSE(pat.match(" foo.bar"));
    CHECK(pat.match("foo.bar"));

    pat = dds::fnmatch::compile("foo.*");
    CHECK(pat.match("foo."));
    auto m = pat.match("foo.b");
    CHECK(m);
    CHECK(pat.match("foo. "));
    CHECK_FALSE(pat.match("foo"));
    CHECK_FALSE(pat.match(" foo.bar"));

    pat = dds::fnmatch::compile("foo.*.cpp");
    for (auto fname : {"foo.bar.cpp", "foo..cpp", "foo.cat.cpp"}) {
        auto m = pat.match(fname);
        CHECK(m);
    }

    for (auto fname : {"foo.cpp", "foo.cpp"}) {
        auto m = pat.match(fname);
        CHECK_FALSE(m);
    }
}
