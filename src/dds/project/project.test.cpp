#include "./project.hpp"

#include <dds/dds.test.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Open a project directory") {
    auto proj = REQUIRES_LEAF_NOFAIL(
        dds::project::open_directory(dds::testing::DATA_DIR / "projects/simple"));
}
