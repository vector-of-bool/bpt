#include "./error.hpp"
#include "./parse.hpp"

#include <bpt/bpt.test.hpp>
#include <bpt/error/try_catch.hpp>

#include <catch2/catch.hpp>
#include <nlohmann/json.hpp>

TEST_CASE("Test parse JSON content") {
    bpt_leaf_try {
        bpt::parse_json_str(R"({foo})");
        FAIL_CHECK("Expected an error");
    }
    bpt_leaf_catch(bpt::e_json_parse_error) {}
    bpt_leaf_catch_all { FAIL_CHECK("Incorrect error: " << diagnostic_info); };

    auto result = REQUIRES_LEAF_NOFAIL(bpt::parse_json_str(R"({"foo": "bar"})"));
    CHECK(result.is_object());
}

TEST_CASE("Test parse JSON5 content") {
    bpt_leaf_try {
        bpt::parse_json5_str("{foo}");
        FAIL_CHECK("Expected an error");
    }
    bpt_leaf_catch(bpt::e_json_parse_error) {}
    bpt_leaf_catch_all { FAIL_CHECK("Incorrect error: " << diagnostic_info); };

    auto result = REQUIRES_LEAF_NOFAIL(bpt::parse_json5_str("{foo: 'bar'}"));
    CHECK(result.is_object());
}
