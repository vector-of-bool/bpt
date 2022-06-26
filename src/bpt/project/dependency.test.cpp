#include "./dependency.hpp"

#include <bpt/bpt.test.hpp>

#include <catch2/catch.hpp>

bpt::crs::version_range_set simple_ver_range(std::string_view from, std::string_view until) {
    return bpt::crs::version_range_set(semver::version::parse(from), semver::version::parse(until));
}

#ifndef _MSC_VER  // MSVC struggles with compiling this test case
TEST_CASE("Parse a shorthand") {
    struct case_ {
        std::string_view        given;
        bpt::project_dependency expect;
    };
    auto [given, expect] = GENERATE(Catch::Generators::values<case_>({
        {
            .given  = "foo@1.2.3",
            .expect = {{"foo"}, simple_ver_range("1.2.3", "2.0.0"), {bpt::name{"foo"}}},
        },
        {
            .given  = "foo~1.2.3  ",
            .expect = {{"foo"}, simple_ver_range("1.2.3", "1.3.0"), {bpt::name{"foo"}}},
        },
        {
            .given  = " foo=1.2.3",
            .expect = {{"foo"}, simple_ver_range("1.2.3", "1.2.4"), {bpt::name{"foo"}}},
        },
        {
            .given  = "foo@1.2.3 using bar , baz",
            .expect = {"foo",
                       simple_ver_range("1.2.3", "2.0.0"),
                       std::vector({bpt::name{"bar"}, bpt::name{"baz"}})},
        },
    }));

    auto dep = REQUIRES_LEAF_NOFAIL(bpt::project_dependency::from_shorthand_string(given));

    CHECK(dep.dep_name == expect.dep_name);
    CHECK(dep.using_ == expect.using_);
    CHECK(dep.acceptable_versions == expect.acceptable_versions);
}
#endif
