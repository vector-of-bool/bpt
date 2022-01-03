#include "./cache.hpp"

#include <dds/dds.test.hpp>
#include <dds/temp.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Create a directory") {
    auto tdir = dds::temporary_dir::create();
    dds::fs::create_directories(tdir.path());

    REQUIRES_LEAF_NOFAIL(dds::crs::cache::open(tdir.path()));
}
