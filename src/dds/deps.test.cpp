#include <dds/deps.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Parse dependency strings") {
    struct case_ {
        std::string depstr;
        std::string name;
        std::string low;
        std::string high;
    };

    auto cur = GENERATE(Catch::Generators::values<case_>({
        {"foo@1.2.3", "foo", "1.2.3", "2.0.0"},
        {"foo=1.2.3", "foo", "1.2.3", "2.0.0"},
        {"foo^1.2.3", "foo", "1.2.3", "2.0.0"},
        {"foo~1.2.3", "foo", "1.2.3", "2.0.0"},
    }));

    auto dep = dds::dependency::parse_depends_string(cur.depstr);
    CHECK(dep.name.str == cur.name);
    CHECK(dep.versions.num_intervals() == 1);
    auto ver_iv = *dep.versions.iter_intervals().begin();
    CHECK(ver_iv.low == semver::version::parse(cur.low));
    CHECK(ver_iv.high == semver::version::parse(cur.high));
}
