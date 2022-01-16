#include "./spdx.hpp"

#include <catch2/catch.hpp>
#include <dds/dds.test.hpp>

auto parse_spdx(std::string_view sv) {
    return REQUIRES_LEAF_NOFAIL(sbs::spdx_license_expression::parse(sv));
    // return sbs::spdx_license_expression::parse(sv);
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
