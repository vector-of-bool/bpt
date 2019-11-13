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
}
