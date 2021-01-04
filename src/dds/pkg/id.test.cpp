#include <dds/pkg/id.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Package package ID strings") {
    struct case_ {
        std::string_view string;
        std::string_view expect_name;
        std::string_view expect_version;
    };
    auto [id_str, exp_name, exp_ver] = GENERATE(Catch::Generators::values<case_>({
        {"foo@1.2.3", "foo", "1.2.3"},
        {"foo@1.2.3-beta", "foo", "1.2.3-beta"},
        {"foo@1.2.3-alpha", "foo", "1.2.3-alpha"},
    }));

    auto pk_id = dds::pkg_id::parse(id_str);
    CHECK(pk_id.to_string() == id_str);
    CHECK(pk_id.name == exp_name);
    CHECK(pk_id.version.to_string() == exp_ver);
}

TEST_CASE("Package ordering") {
    enum order {
        less_than,
        equivalent_to,
        greater_than,
    };
    struct case_ {
        std::string_view lhs_pkg;
        order            ord;
        std::string_view rhs_pkg;
    };

    auto [lhs_str, ord, rhs_str] = GENERATE(Catch::Generators::values<case_>({
        {"foo@1.2.3", greater_than, "bar@1.2.3"},
        {"foo@1.2.3", greater_than, "foo@1.2.2"},
        {"foo@1.2.3", equivalent_to, "foo@1.2.3"},
        {"foo@1.2.3", equivalent_to, "foo@1.2.3+build-meta"},
        {"foo@1.2.3", greater_than, "foo@1.2.3-alpha"},
        {"foo@1.2.3-alpha.1", greater_than, "foo@1.2.3-alpha"},
        {"foo@1.2.3-alpha.2", greater_than, "foo@1.2.3-alpha.1"},
        {"foo@1.2.3-beta.2", greater_than, "foo@1.2.3-alpha.1"},
        {"foo@0.1.2-alpha", less_than, "foo@1.0.0"},
    }));

    auto lhs = dds::pkg_id::parse(lhs_str);
    auto rhs = dds::pkg_id::parse(rhs_str);

    if (ord == less_than) {
        CHECK(lhs < rhs);
    } else if (ord == equivalent_to) {
        CHECK(lhs == rhs);
    } else if (ord == greater_than) {
        CHECK_FALSE(lhs == rhs);
        CHECK_FALSE(lhs < rhs);
    }
}