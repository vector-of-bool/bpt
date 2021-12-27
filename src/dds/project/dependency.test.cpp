#include "./dependency.hpp"

#include <dds/dds.test.hpp>

#include <catch2/catch.hpp>

dds::crs::version_range_set simple_ver_range(std::string_view from, std::string_view until) {
    return dds::crs::version_range_set(semver::version::parse(from), semver::version::parse(until));
}

TEST_CASE("Parse a shorthand") {
    struct case_ {
        std::string_view        given;
        dds::project_dependency expect;
    };

    auto [given, expect] = GENERATE(Catch::Generators::values<case_>({
        {
            .given  = "foo@1.2.3",
            .expect = {{"foo"}, simple_ver_range("1.2.3", "2.0.0")},
        },
        {
            .given  = "foo~1.2.3  ",
            .expect = {{"foo"}, simple_ver_range("1.2.3", "1.3.0")},
        },
        {
            .given  = " foo=1.2.3",
            .expect = {{"foo"}, simple_ver_range("1.2.3", "1.2.4")},
        },
        {
            .given  = "foo@1.2.3; for: test",
            .expect = {"foo", simple_ver_range("1.2.3", "2.0.0"), dds::crs::usage_kind::test},
        },
        {
            .given  = "foo@1.2.3; for: test; use: bar , baz",
            .expect = {"foo",
                       simple_ver_range("1.2.3", "2.0.0"),
                       dds::crs::usage_kind::test,
                       std::vector({dds::name{"bar"}, dds::name{"baz"}})},
        },
    }));

    auto dep = REQUIRES_LEAF_NOFAIL(dds::project_dependency::from_shorthand_string(given));

    CHECK(dep.dep_name == expect.dep_name);
    CHECK(dep.explicit_uses == expect.explicit_uses);
    CHECK(dep.kind == expect.kind);
    CHECK(dep.acceptable_versions == expect.acceptable_versions);
}
