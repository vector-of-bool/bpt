#include "./dym.hpp"

#include <catch2/catch.hpp>

TEST_CASE("Basic string distance") {
    CHECK(bpt::lev_edit_distance("a", "a") == 0);
    CHECK(bpt::lev_edit_distance("a", "b") == 1);
    CHECK(bpt::lev_edit_distance("aa", "a") == 1);
}

TEST_CASE("Find the 'did-you-mean' candidate") {
    auto cand = bpt::did_you_mean("food", {"foo", "bar"});
    CHECK(cand == "foo");
    cand = bpt::did_you_mean("eatable", {"edible", "tangible"});
    CHECK(cand == "edible");
}
