#pragma once

#include <dds/error/result_fwd.hpp>

#include <json5/data.hpp>
#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <string>

namespace dds {

struct e_json_parse_error {
    std::string value;
};

result<nlohmann::json> parse_json_str(std::string_view) noexcept;
result<nlohmann::json> parse_json_file(std::filesystem::path const&) noexcept;
result<json5::data>    parse_json5_str(std::string_view) noexcept;
result<json5::data>    parse_json5_file(std::filesystem::path const&) noexcept;

}  // namespace dds
