#include <dds/build/deps.hpp>

#include <catch2/catch.hpp>

auto path_vec = [](auto... args) { return std::vector<dds::fs::path>{args...}; };

TEST_CASE("Parse Makefile deps") {
    auto deps = dds::parse_mkfile_deps_str("foo.o: bar.c");
    CHECK(deps.output == "foo.o");
    CHECK(deps.inputs == path_vec("bar.c"));
    // Newline is okay
    deps = dds::parse_mkfile_deps_str("foo.o: bar.c \\\n baz.c");
    CHECK(deps.output == "foo.o");
    CHECK(deps.inputs == path_vec("bar.c", "baz.c"));
}

TEST_CASE("Invalid deps") {
    // Invalid deps does not terminate. This will generate an error message in
    // the logs, but it is a non-fatal error that we can recover from.
    auto deps = dds::parse_mkfile_deps_str("foo.o : cat");
    CHECK(deps.output.empty());
    CHECK(deps.inputs.empty());
    deps = dds::parse_mkfile_deps_str("foo.c");
    CHECK(deps.output.empty());
    CHECK(deps.inputs.empty());
}