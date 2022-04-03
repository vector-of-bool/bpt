#include "./spdx.hpp"

#include <bpt/bpt.test.hpp>
#include <catch2/catch.hpp>

auto parse_spdx(std::string_view sv) {
    return REQUIRES_LEAF_NOFAIL(bpt::spdx_license_expression::parse(sv));
    // return bpt::spdx_license_expression::parse(sv);
}

TEST_CASE("Parse a license string") {
    auto expr = parse_spdx("BSL-1.0");

    CHECK(expr.to_string() == "BSL-1.0");

    expr = parse_spdx("BSL-1.0+");
    CHECK(expr.to_string() == "BSL-1.0+");

    auto conjunct = parse_spdx("BSL-1.0 AND MPL-1.0");
    CHECK(conjunct.to_string() == "BSL-1.0 AND MPL-1.0");

    conjunct = parse_spdx("(BSL-1.0 AND MPL-1.0)");
    CHECK(conjunct.to_string() == "(BSL-1.0 AND MPL-1.0)");
}
