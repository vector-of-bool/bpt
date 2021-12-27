#pragma once

#include <dds/error/result_fwd.hpp>

#include <string>

namespace lm {

struct e_invalid_usage_string {
    std::string value;
};

struct usage {
    std::string namespace_;
    std::string name;

    auto operator<=>(const usage&) const noexcept = default;
};

dds::result<usage> split_usage_string(std::string_view);

}  // namespace lm