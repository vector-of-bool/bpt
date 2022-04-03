#include <bpt/toolchain/errors.hpp>
#include <bpt/toolchain/toolchain.hpp>

#include <bpt/error/doc_ref.hpp>
#include <bpt/error/try_catch.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Builtin toolchains reject multiple standards") {
    dds_leaf_try {
        dds::toolchain::get_builtin("c++11:c++14:gcc");
        FAIL_CHECK("Did not throw");
    }
    dds_leaf_catch(bpt::e_doc_ref ref, bpt::e_builtin_toolchain_str given) {
        CHECK(ref.value == "err/invalid-builtin-toolchain.html");
        CHECK(given.value == "c++11:c++14:gcc");
    }
    dds_leaf_catch_all { FAIL_CHECK("Unknown error:" << diagnostic_info); };
}
