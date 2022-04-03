#pragma once

#include <yaml-cpp/node/node.h>

#include <filesystem>
#include <string_view>

namespace sbs {

YAML::Node parse_yaml_file(const std::filesystem::path&);
YAML::Node parse_yaml_string(std::string_view);

}  // namespace sbs