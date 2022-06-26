#include "./project.hpp"

#include <bpt/bpt.test.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Open a project directory") {
    auto proj = REQUIRES_LEAF_NOFAIL(
        bpt::project::open_directory(bpt::testing::DATA_DIR / "projects/simple"));
}
