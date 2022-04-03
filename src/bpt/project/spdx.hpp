#pragma once

#include <memory>
#include <span>
#include <string>

namespace sbs {

struct e_spdx_license_str {
    std::string value;
};

struct e_bad_spdx_expression {
    std::string value;
};

struct spdx_license_info {
    std::string id;
    std::string name;

    static const std::span<const spdx_license_info> all;
};

struct spdx_exception_info {
    std::string id;
    std::string name;

    static const std::span<const spdx_exception_info> all;
};

class spdx_license_expression {
    struct impl;

    std::shared_ptr<impl> _impl;

    explicit spdx_license_expression(std::shared_ptr<impl> i) noexcept
        : _impl(i) {}

public:
    static spdx_license_expression parse(std::string_view sv);

    std::string to_string() const noexcept;
};

}  // namespace sbs
