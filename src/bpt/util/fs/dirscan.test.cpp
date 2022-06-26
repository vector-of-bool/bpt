#include "./dirscan.hpp"

#include <neo/ranges.hpp>

#include <bpt/error/result.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Create a simple scanner") {
    auto this_dir = bpt::fs::path(__FILE__).lexically_normal().parent_path();
    auto db       = bpt::unique_database::open(":memory:");
    REQUIRE(db);
    auto finder = bpt::file_collector::create(*db);
    CHECK_FALSE(finder.has_cached(this_dir));
    auto found = finder.collect(this_dir) | neo::to_vector;
    CHECK_FALSE(found.empty());
    CHECK(finder.has_cached(this_dir));
    finder.forget(this_dir);
    CHECK_FALSE(finder.has_cached(this_dir));
}
