#include <fansi/writer.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Write a string") {
    fansi::text_writer wr;
    wr.write("foo");
    CHECK(wr.string() == "foo");
}
