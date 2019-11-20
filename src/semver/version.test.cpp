#include "./version.hpp"

#include <catch2/catch.hpp>

TEST_CASE("Parsing") {
    auto v1 = semver::version::parse("1.2.3");
    CHECK(v1.major == 1);
    CHECK(v1.minor == 2);
    CHECK(v1.patch == 3);

    CHECK(v1.to_string() == "1.2.3");
    v1.patch = 55;
    CHECK(v1.to_string() == "1.2.55");
    v1.major = 999999;
    CHECK(v1.to_string() == "999999.2.55");

    v1 = semver::version::parse("1.2.3-r1");
    CHECK(v1.prerelease == semver::prerelease::parse("r1"));
    CHECK(v1.to_string() == "1.2.3-r1");

    v1 = semver::version::parse("1.2.3-3");
    CHECK(v1.prerelease == semver::prerelease::parse("3"));

    v1 = semver::version::parse("1.2.3-0");
    CHECK(v1.prerelease == semver::prerelease::parse("0"));

    v1 = semver::version::parse("1.2.3--fasdf");
    CHECK(v1.prerelease == semver::prerelease::parse("-fasdf"));

    v1 = semver::version::parse("1.2.3-foo.bar");
    CHECK(v1.prerelease == semver::prerelease::parse("foo.bar"));

    v1 = semver::version::parse("1.2.3-foo.bar+cats");
    CHECK(v1.prerelease == semver::prerelease::parse("foo.bar"));
    CHECK(v1.build_metadata.idents() == semver::ident::parse_dotted_seq("cats"));
}

TEST_CASE("Compare versions") {
    using semver::order;
    struct case_ {
        std::string_view lhs;
        std::string_view rhs;
        order            expect_ord;
    };

    case_ cases[] = {
        {"1.2.3", "1.2.3", order::equivalent},
        {"1.2.3-alpha", "1.2.3-alpha", order::equivalent},
        {"1.2.3-alpha", "1.2.3", order::less},
        {"1.2.3-alpha.1", "1.2.3-beta", order::less},
        {"1.2.3-alpha.1", "1.2.3-alpha.2", order::less},
        {"1.2.3-alpha.4", "1.2.3-alpha.2", order::greater},
        {"1.2.1", "1.2.2-alpha.2", order::less},
        {"1.2.1", "1.2.1+foo", order::equivalent}, // Build metadata has no effect
    };

    for (auto [lhs, rhs, exp] : cases) {
        INFO("Compare version '" << lhs << "' to '" << rhs << "'");
        auto lhs_ver = semver::version::parse(lhs);
        auto rhs_ver = semver::version::parse(rhs);
        auto actual  = semver::compare(lhs_ver, rhs_ver);
        CHECK(actual == exp);
    }
}

TEST_CASE("Invalid versions") {
    struct invalid_version {
        std::string str;
        int         bad_offset;
    };
    invalid_version versions[] = {
        {"", 0},
        {"1.", 2},
        {"1.2", 3},
        {"1.2.", 4},
        {"1.e", 2},
        {"lol", 0},
        {"1e.3.1", 1},
        {"1.2.5-", 6},
        {"1.2.5-02", 6},
        {"1.2.3-1..3", 8},
    };
    for (auto&& [str, bad_offset] : versions) {
        INFO("Checking for failure while parsing bad version string '" << str << "'");
        try {
            auto ver = semver::version::parse(str);
            FAIL_CHECK("Parsing didn't throw! Produced version: " << ver.to_string());
        } catch (const semver::invalid_version& e) {
            CHECK(e.offset() == bad_offset);
        } catch (const semver::invalid_ident&) {
            // Nothing to check, but a valid error
        }
    }
}