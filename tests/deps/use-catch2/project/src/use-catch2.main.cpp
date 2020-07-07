#include <catch2/catch_with_main.hpp>

TEST_CASE("I am a simple test case") {
    CHECK((2 + 2) == 4);
    CHECK_FALSE((2 + 2) == 5);
}
