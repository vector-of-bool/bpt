#pragma once

#include "./package_info.hpp"

namespace dds {

std::vector<package_info> parse_packages_json(std::string_view);

}  // namespace dds
