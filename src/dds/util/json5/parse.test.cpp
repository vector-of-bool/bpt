#include "./error.hpp"
#include "./parse.hpp"

#include <dds/dds.test.hpp>
#include <dds/error/try_catch.hpp>

#include <catch2/catch.hpp>
#include <nlohmann/json.hpp>

TEST_CASE("Test parse JSON content") {
    dds_leaf_try {
        dds::parse_json_str(R"({foo})");
        FAIL_CHECK("Expected an error");
    }
    dds_leaf_catch(dds::e_json_parse_error) {}
    dds_leaf_catch_all { FAIL_CHECK("Incorrect error: " << diagnostic_info); };

    auto result = REQUIRES_LEAF_NOFAIL(dds::parse_json_str(R"({"foo": "bar"})"));
    CHECK(result.is_object());
}

TEST_CASE("Test parse JSON5 content") {
    dds_leaf_try {
        dds::parse_json5_str("{foo}");
        FAIL_CHECK("Expected an error");
    }
    dds_leaf_catch(dds::e_json_parse_error) {}
    dds_leaf_catch_all { FAIL_CHECK("Incorrect error: " << diagnostic_info); };

    auto result = REQUIRES_LEAF_NOFAIL(dds::parse_json5_str("{foo: 'bar'}"));
    CHECK(result.is_object());
}
