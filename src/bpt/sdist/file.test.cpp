#include <bpt/sdist/file.hpp>

#include <catch2/catch.hpp>

using bpt::source_kind;

TEST_CASE("Infer source kind") {
    using bpt::infer_source_kind;
    auto k = infer_source_kind("foo.h");
    CHECK(k == source_kind::header);
    CHECK(infer_source_kind("foo.hpp") == source_kind::header);
    CHECK_FALSE(infer_source_kind("foo.txt"));  // Not a source file extension

    CHECK(infer_source_kind("foo.hh") == source_kind::header);
}
