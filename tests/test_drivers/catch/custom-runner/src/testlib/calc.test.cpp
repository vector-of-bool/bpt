#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

#include <testlib/calc.hpp>

TEST_CASE("A simple test case") {
    CHECK_FALSE(false);
    CHECK(2 == 2);
    CHECK(1 != 4);
    CHECK(stuff::calculate(3, 11) == 42);
}

int main(int argc, char** argv) {
    // We provide our own runner
    return Catch::Session().run(argc, argv);
}