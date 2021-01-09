#pragma once

#include <dds/error/result_fwd.hpp>

#include <string>

namespace dds {

enum class invalid_name_reason {
    empty,
    capital,
    initial_not_alpha,
    double_punct,
    end_punct,
    invalid_char,
    whitespace,
};

struct e_name_str {
    std::string value;
};

std::string_view invalid_name_reason_str(invalid_name_reason) noexcept;

struct name {
    std::string str;

    // Parse a package name, ensuring it is a valid package name string
    static result<name> from_string(std::string_view str) noexcept;

    bool operator==(const name& o) const noexcept { return str == o.str; }
    bool operator!=(const name& o) const noexcept { return str != o.str; }
    bool operator<(const name& o) const noexcept { return str < o.str; }
};

}  // namespace dds
