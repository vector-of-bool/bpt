#pragma once

#include <string>
#include <string_view>

namespace bpt {

void write_error_marker(std::string_view) noexcept;

struct e_error_marker {
    std::string value;
};

}  // namespace bpt