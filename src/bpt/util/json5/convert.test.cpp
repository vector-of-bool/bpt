#include "./convert.hpp"

#include <nlohmann/json.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Convert to JSON5") {
    auto data = nlohmann::json::object({
        {"name", "Joe"},
        {"address", "Main Street"},
    });

    auto j5 = bpt::nlohmann_json_as_json5(data);
    REQUIRE(j5.is_object());
    auto& map = j5.as_object();

    auto it = map.find("name");
    CHECKED_IF(it != map.cend()) {
        CHECKED_IF(it->second.is_string()) { REQUIRE(it->second.as_string() == "Joe"); }
    }

    it = map.find("address");
    CHECKED_IF(it != map.cend()) {
        CHECKED_IF(it->second.is_string()) { CHECK(it->second.as_string() == "Main Street"); }
    }
}
