#pragma once

#include <filesystem>
#include <string>

namespace bpt {

struct e_parse_yaml_file_path {
    std::filesystem::path value;
};

struct e_parse_yaml_string {
    std::string value;
};

struct e_yaml_parse_error {
    std::string value;
};

struct e_yaml_unknown_tag {
    std::string value;
};

struct e_yaml_tag {
    std::string value;
};

struct e_yaml_invalid_spelling {
    std::string value;
};

}  // namespace bpt