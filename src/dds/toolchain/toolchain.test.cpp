#include <dds/toolchain/toolchain.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Builtin toolchains reject multiple standards") {
    const std::optional<dds::toolchain> parsed = dds::toolchain::get_builtin("c++11:c++14:gcc");
    CHECK_FALSE(parsed.has_value());
}
