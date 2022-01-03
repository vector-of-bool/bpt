#pragma once

#include <filesystem>

namespace dds {

struct e_sdist_from_directory {
    std::filesystem::path value;
};

struct e_missing_pkg_json {
    std::filesystem::path value;
};

struct e_missing_project_json5 {
    std::filesystem::path value;
};

}  // namespace dds
