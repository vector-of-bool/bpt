#pragma once

#include <dds/error/result_fwd.hpp>

#include <iosfwd>
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

std::ostream& operator<<(std::ostream& out, invalid_name_reason);

struct e_name_str {
    std::string value;
};

std::string_view invalid_name_reason_str(invalid_name_reason) noexcept;

struct name {
    std::string str;

    // Parse a package name, ensuring it is a valid package name string
    static result<name> from_string(std::string_view str) noexcept;

    auto operator<=>(const name&) const noexcept = default;

    void write_to(std::ostream&) const noexcept;

    friend std::ostream& operator<<(std::ostream& o, const name& self) noexcept {
        self.write_to(o);
        return o;
    }
};

}  // namespace dds
