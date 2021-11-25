#include "./parse.hpp"

#include <boost/leaf.hpp>
#include <boost/leaf/context.hpp>
#include <catch2/catch.hpp>
#include <nlohmann/json.hpp>

#include <dds/error/result.hpp>

TEST_CASE("Test parse JSON content") {
    boost::leaf::context<dds::e_json_parse_error> ctx;
    ctx.activate();
    auto result = dds::parse_json_str(R"({foo})");
    REQUIRE_FALSE(result);
    ctx.deactivate();
    ctx.handle_error<void>(
        result.error(),
        [](dds::e_json_parse_error) {
            // okay
        },
        [] { FAIL_CHECK("Incorrect error"); });

    result = dds::parse_json_str(R"({"foo": "bar"})");
    REQUIRE(result);
    CHECK(result->is_object());
}

TEST_CASE("Test parse JSON5 content") {
    boost::leaf::context<dds::e_json_parse_error> ctx;
    ctx.activate();
    auto result = dds::parse_json5_str(R"({foo})");
    REQUIRE_FALSE(result);
    ctx.deactivate();
    ctx.handle_error<void>(
        result.error(),
        [](dds::e_json_parse_error) {
            // okay
        },
        [] { FAIL_CHECK("Incorrect error"); });

    result = dds::parse_json5_str(R"({foo: 'bar'})");
    REQUIRE(result);
    CHECK(result->is_object());
}
