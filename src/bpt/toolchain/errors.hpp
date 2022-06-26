#pragma once

#include <filesystem>
#include <string>

namespace bpt {

struct e_toolchain_filepath {
    std::filesystem::path value;
};

struct e_builtin_toolchain_str {
    std::string value;
};

}  // namespace bpt