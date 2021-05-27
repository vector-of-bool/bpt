#include <catch2/catch_with_main.hpp>

#include <testlib/calc.hpp>

TEST_CASE("A simple test case") {
    CHECK_FALSE(false);
    CHECK(2 == 2);
    CHECK(1 != 4);
    CHECK(stuff::calculate(3, 11) == 42);
}
