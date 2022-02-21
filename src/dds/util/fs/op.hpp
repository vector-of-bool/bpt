#pragma once

#include <filesystem>

namespace dds::inline file_utils {

bool file_exists(const std::filesystem::path&);

}  // namespace dds::inline file_utils