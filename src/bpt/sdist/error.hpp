#pragma once

#include <filesystem>

namespace bpt {

struct e_sdist_from_directory {
    std::filesystem::path value;
};

struct e_missing_pkg_json {
    std::filesystem::path value;
};

struct e_missing_project_yaml {
    std::filesystem::path value;
};

}  // namespace bpt
