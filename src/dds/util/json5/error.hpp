#pragma once

#include <string>

namespace dds {

struct e_json_string {
    std::string value;
};

struct e_json5_string {
    std::string value;
};

struct e_json_parse_error {
    std::string value;
};

}  // namespace dds