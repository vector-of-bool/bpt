#pragma once

#include "./package_info.hpp"

#include <vector>

namespace dds {

const std::vector<package_info>& init_catalog_packages() noexcept;

}  // namespace dds