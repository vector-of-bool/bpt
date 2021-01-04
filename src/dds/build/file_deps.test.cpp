#include <dds/build/file_deps.hpp>

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

    deps = dds::parse_mkfile_deps_str(
        "/some-path/Ю́рий\\ Алексе́евич\\ Гага́рин/build/obj/foo.main.cpp.o: \\\n"
        "  /foo.main.cpp \\\n"
        "  /stdc-predef.h\n");
    CHECK(deps.output == "/some-path/Ю́рий Алексе́евич Гага́рин/build/obj/foo.main.cpp.o");
    CHECK(deps.inputs == path_vec("/foo.main.cpp", "/stdc-predef.h"));
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

TEST_CASE("Parse MSVC deps") {
    auto mscv_output = R"(
Note: including file:    C:\foo\bar\filepath/thing.hpp
Note: including file:  C:\foo\bar\filepath/baz.h
Note: including file:      C:\foo\bar\filepath/quux.h
    Note: including file:   C:\foo\bar\filepath/cats/quux.h
Other line
    indented line
Something else
)";

    auto  res        = dds::parse_msvc_output_for_deps(mscv_output, "Note: including file:");
    auto& deps       = res.deps_info;
    auto  new_output = res.cleaned_output;
    CHECK(new_output == "\nOther line\n    indented line\nSomething else\n");
    CHECK(deps.inputs
          == std::vector<dds::fs::path>({
              "C:\\foo\\bar\\filepath/thing.hpp",
              "C:\\foo\\bar\\filepath/baz.h",
              "C:\\foo\\bar\\filepath/quux.h",
              "C:\\foo\\bar\\filepath/cats/quux.h",
          }));
}