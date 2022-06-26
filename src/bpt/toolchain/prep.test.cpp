#include "./prep.hpp"

#include <catch2/catch.hpp>

TEST_CASE("Hash the toolchain") {
    bpt::toolchain_prep prep;
    CHECK(prep.compute_hash() == 0x174b6917312d24b2);

    prep.c_compile = {"gcc"};
    CHECK(prep.compute_hash() == 0x5ba3168895eae55a);
    // Some compiler options are pruned as part of the hash
    prep.c_compile = {"gcc", "-fdiagnostics-color"};
    CHECK(prep.compute_hash() == 0x5ba3168895eae55a);
}
