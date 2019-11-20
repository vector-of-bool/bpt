#include "./prerelease.hpp"

#include <catch2/catch.hpp>

TEST_CASE("Parse a prerelease") {
    auto pre = semver::prerelease::parse("foo.bar");
    CHECK(pre.idents() == semver::ident::parse_dotted_seq("foo.bar"));
    pre = pre.parse("foo.0");
    CHECK(pre.idents() == semver::ident::parse_dotted_seq("foo.0"));
}

TEST_CASE("Invalid prereleases") {
    std::string_view bad_tags[] = {
        "foo.",        // Just plain bad
        "foo.122.03",  // Leading zero
    };
    for (auto bad : bad_tags) {
        CHECK_THROWS_AS(semver::prerelease::parse(bad), semver::invalid_ident);
    }
}

TEST_CASE("Compare prereleases") {
    using semver::order;
    struct case_ {
        std::string lhs;
        std::string rhs;
        order       expected_ord;
    };

    case_ cases[] = {
        {"foo.bar", "foo.bar", order::equivalent},
        {"foo", "foo.bar", order::less},
        {"foo.bar", "foo", order::greater},
        {"foo", "foo", order::equivalent},
        {"foo.1", "foo.bar", order::less},  // numeric is less
        {"foo.foo", "foo.bar", order::greater},
        {"alpha", "beta", order::less},
        {"alpha.1", "alpha.2", order::less},
    };

    for (auto& [lhs, rhs, exp] : cases) {
        INFO("Comparing prerelease '" << lhs << "' to '" << rhs << "'");
        auto lhs_pre = semver::prerelease::parse(lhs);
        auto rhs_pre = semver::prerelease::parse(rhs);
        auto actual  = semver::compare(lhs_pre, rhs_pre);
        CHECK(actual == exp);
    }
}