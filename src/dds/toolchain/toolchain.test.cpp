#include <dds/toolchain/errors.hpp>
#include <dds/toolchain/toolchain.hpp>

#include <dds/error/doc_ref.hpp>
#include <dds/error/try_catch.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Builtin toolchains reject multiple standards") {
    dds_leaf_try {
        dds::toolchain::get_builtin("c++11:c++14:gcc");
        FAIL_CHECK("Did not throw");
    }
    dds_leaf_catch(sbs::e_doc_ref ref, sbs::e_builtin_toolchain_str given) {
        CHECK(ref.value == "err/invalid-builtin-toolchain.html");
        CHECK(given.value == "c++11:c++14:gcc");
    }
    dds_leaf_catch_all { FAIL_CHECK("Unknown error:" << diagnostic_info); };
}
