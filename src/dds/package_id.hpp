#pragma once

#include <semver/version.hpp>

#include <string>
#include <string_view>
#include <tuple>

namespace dds {

struct package_id {
    std::string     name;
    semver::version version;

    static package_id parse(std::string_view);
    std::string       to_string() const noexcept;

    auto tie() const noexcept { return std::tie(name, version); }

    friend bool operator<(const package_id& lhs, const package_id& rhs) noexcept {
        return lhs.tie() < rhs.tie();
    }
    friend bool operator==(const package_id& lhs, const package_id& rhs) noexcept {
        return lhs.tie() == rhs.tie();
    }
};

}  // namespace dds