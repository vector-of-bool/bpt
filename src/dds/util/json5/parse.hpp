#pragma once

#include <json5/data.hpp>
#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <string>

namespace dds {

nlohmann::json parse_json_str(std::string_view);
nlohmann::json parse_json_file(std::filesystem::path const&);
json5::data    parse_json5_str(std::string_view);
json5::data    parse_json5_file(std::filesystem::path const&);

}  // namespace dds
