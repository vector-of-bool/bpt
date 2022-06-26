#pragma once

#include <json5/data.hpp>
#include <nlohmann/json_fwd.hpp>

namespace bpt {

nlohmann::json json5_as_nlohmann_json(const json5::data& data) noexcept;
json5::data    nlohmann_json_as_json5(const nlohmann::json&) noexcept;

}  // namespace bpt
