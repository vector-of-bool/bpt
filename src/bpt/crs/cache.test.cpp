#include "./cache.hpp"

#include <bpt/bpt.test.hpp>
#include <bpt/temp.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Create a directory") {
    auto tdir = bpt::temporary_dir::create();
    bpt::fs::create_directories(tdir.path());

    REQUIRES_LEAF_NOFAIL(bpt::crs::cache::open(tdir.path()));
}
