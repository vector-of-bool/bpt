#include <dds/util/glob.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Simple glob") {
    auto this_dir = dds::fs::path(__FILE__).parent_path();
    auto glob     = dds::glob::compile("*.test.cpp");
    ::setlocale(LC_ALL, ".utf8");

    auto it = glob.scan_from(this_dir);
    for (; it != glob.end(); ++it) {
        auto&& el = *it;
    }

    int n_found = 0;
    for (auto found : glob.scan_from(this_dir)) {
        ++n_found;
    }
    CHECK(n_found > 0);

    n_found = 0;
    for (auto found : dds::glob::compile("glob.test.cpp").scan_from(this_dir)) {
        n_found++;
    }
    CHECK(n_found == 1);

    auto me_it = dds::glob::compile("src/**/glob.test.cpp").begin();
    REQUIRE(!me_it.at_end());
    ++me_it;
    CHECK(me_it.at_end());

    auto all_tests = dds::glob::compile("src/**/*.test.cpp");
    n_found        = 0;
    for (auto f : all_tests) {
        n_found += 1;
    }
    CHECK(n_found > 10);
    CHECK(n_found < 1000);  // If we have more than 1000 .test files, that's crazy
}

TEST_CASE("Check globs") {
    auto glob = dds::glob::compile("foo/bar*/baz");
    CHECK(glob.match("foo/bar/baz"));
    CHECK(glob.match("foo/barffff/baz"));
    CHECK_FALSE(glob.match("foo/bar"));
    CHECK_FALSE(glob.match("foo/ffbar/baz"));
    CHECK_FALSE(glob.match("foo/bar/bazf"));
    CHECK_FALSE(glob.match("foo/bar/"));

    glob = dds::glob::compile("foo/**/bar.txt");
    CHECK(glob.match("foo/bar.txt"));
    CHECK(glob.match("foo/thing/bar.txt"));
    CHECK(glob.match("foo/thing/another/bar.txt"));
    CHECK_FALSE(glob.match("foo/fail"));
    CHECK_FALSE(glob.match("foo/bar.txtf"));
    CHECK_FALSE(glob.match("foo/bar.txt/f"));
    CHECK_FALSE(glob.match("foo/fbar.txt"));
    CHECK_FALSE(glob.match("foo/thing/fail"));
    CHECK_FALSE(glob.match("foo/thing/another/fail"));
    CHECK_FALSE(glob.match("foo/thing/bar.txt/fail"));
    CHECK_FALSE(glob.match("foo/bar.txt/fail"));

    glob = dds::glob::compile("foo/**/bar/**/baz.txt");
    CHECK(glob.match("foo/bar/baz.txt"));
    CHECK(glob.match("foo/thing/bar/baz.txt"));
    CHECK(glob.match("foo/thing/bar/baz.txt"));
    CHECK(glob.match("foo/thing/bar/thing/baz.txt"));
    CHECK(glob.match("foo/bar/thing/baz.txt"));
    CHECK(glob.match("foo/bar/baz/baz.txt"));

    glob = dds::glob::compile("doc/**");
    CHECK(glob.match("doc/something.txt"));
}
