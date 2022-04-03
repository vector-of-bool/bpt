#pragma once

#include <bpt/error/result_fwd.hpp>

#include <iosfwd>
#include <string>

namespace lm {

struct e_invalid_usage_string {
    std::string value;
};

struct usage {
    std::string namespace_;
    std::string name;

    auto operator<=>(const usage&) const noexcept = default;

    void write_to(std::ostream&) const noexcept;

    friend std::ostream& operator<<(std::ostream& out, const usage& self) noexcept {
        self.write_to(out);
        return out;
    }
};

dds::result<usage> split_usage_string(std::string_view);

}  // namespace lm