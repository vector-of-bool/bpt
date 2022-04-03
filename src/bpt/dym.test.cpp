#include "./dym.hpp"

#include <catch2/catch.hpp>

TEST_CASE("Basic string distance") {
    CHECK(dds::lev_edit_distance("a", "a") == 0);
    CHECK(dds::lev_edit_distance("a", "b") == 1);
    CHECK(dds::lev_edit_distance("aa", "a") == 1);
}

TEST_CASE("Find the 'did-you-mean' candidate") {
    auto cand = dds::did_you_mean("food", {"foo", "bar"});
    CHECK(cand == "foo");
    cand = dds::did_you_mean("eatable", {"edible", "tangible"});
    CHECK(cand == "edible");
}
