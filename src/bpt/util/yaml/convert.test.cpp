#include "./convert.hpp"
#include "./parse.hpp"

#include "./errors.hpp"

#include <bpt/bpt.test.hpp>
#include <bpt/error/try_catch.hpp>

#include <catch2/catch.hpp>

static json5::data parse_and_convert(std::string_view data) {
    auto node = bpt::parse_yaml_string(data);
    return bpt::yaml_as_json5_data(node);
}

TEST_CASE("Convert some simple scalars") {
    auto [given, expect] = GENERATE(Catch::Generators::table<std::string, json5::data>({
        {"null", json5::data::null_type{}},
        {"", json5::data::null_type{}},
        {"false", false},
        // No "no" as bool:
        {"no", "no"},
        // Unless they ask for it
        {"!!bool no", false},
        {"!!str false", "false"},
        {"'false'", "false"},
        {"\"false\"", "false"},
        {"!!null lol", nullptr},
        {"!!int 4", 4.0},
        {"!!float 4", 4.0},
    }));

    CAPTURE(given);
    auto data = REQUIRES_LEAF_NOFAIL(parse_and_convert(given));
    CHECK(data == expect);
}
