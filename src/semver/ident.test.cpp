#include <semver/ident.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Parse identifiers") {
    struct case_ {
        std::string        str;
        semver::ident_kind expect_kind;
    };
    case_ cases[] = {
        {"foo", semver::ident_kind::alphanumeric},
        {"12", semver::ident_kind::numeric},
        {"03412", semver::ident_kind::digits},
        {"0", semver::ident_kind::numeric},
        {"00", semver::ident_kind::digits},
    };
    for (auto [str, expect] : cases) {
        INFO("Checking parsing of '" << str << "'");
        auto id = semver::ident(str);
        CHECK(id.string() == str);
        CHECK(id.kind() == expect);
    }
}

TEST_CASE("Invalid identifiers") {
#define BAD_IDENT(x) CHECK_THROWS_AS(semver::ident(x), semver::invalid_ident)
    BAD_IDENT("");
    BAD_IDENT("=");
    BAD_IDENT("asdf-_");
    BAD_IDENT(".");
    BAD_IDENT("  ");
    BAD_IDENT("124a[");
}

TEST_CASE("Ident comparison") {
    using semver::order;
    struct comp_test {
        std::string lhs;
        std::string rhs;
        order       expect_ordering;
    };
    comp_test comparisons[] = {
        {"foo", "bar", order::greater},
        {"foo", "foo", order::equivalent},
        {"bar", "foo", order::less},
        {"12", "333", order::less},
        {"fooood", "3", order::greater},
        {"0", "0", order::equivalent},
        {"34", "f", order::less},
        {"aaaaaaaaaaa", "z", order::less},
    };
    for (auto [lhs, rhs, expect] : comparisons) {
        auto lhs_id = semver::ident(lhs);
        auto rhs_id = semver::ident(rhs);
        INFO("Comparing '" << lhs << "' to '" << rhs << "'");
        auto result = semver::compare(lhs_id, rhs_id);
        CHECK(result == expect);
    }
}

TEST_CASE("Parse dotted sequence") {
    struct case_ {
        std::string              str;
        std::vector<std::string> expected;
    };
    case_ cases[] = {
        {"foo", {"foo"}},
        {"foo.bar", {"foo", "bar"}},
    };
    for (auto& [str, expected] : cases) {
        INFO("Parsing dotted-ident-sequence '" << str << "'");
        auto actual = semver::ident::parse_dotted_seq(str);
        REQUIRE(actual.size() == expected.size());
    }
}

TEST_CASE("Invalid dotted sequence") {
    std::string_view bad_seqs[] = {
        "",
        ".",
        ".foo",
        "foo.bar.",
        "foo..bar",
    };
    for (auto s : bad_seqs) {
        INFO("Checking bad ident sequence '" << s << "'");
        CHECK_THROWS_AS(semver::ident::parse_dotted_seq(s), semver::invalid_ident);
    }
}