#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace semver {

class invalid_version : std::runtime_error {
    std::ptrdiff_t _offset = 0;

public:
    invalid_version(std::ptrdiff_t n)
        : runtime_error("Invalid version number")
        , _offset(n) {}

    auto offset() const noexcept { return _offset; }
};

struct version {
    int major = 0;
    int minor = 0;
    int patch = 0;

    static version parse(std::string_view s);

    std::string to_string() const noexcept;

    auto tie() const noexcept { return std::tie(major, minor, patch); }
    auto tie() noexcept { return std::tie(major, minor, patch); }
};

inline bool operator!=(const version& lhs, const version& rhs) noexcept {
    return lhs.tie() != rhs.tie();
}

inline bool operator<(const version& lhs, const version& rhs) noexcept {
    return lhs.tie() < rhs.tie();
}

inline std::string to_string(const version& ver) noexcept { return ver.to_string(); }

}  // namespace semver
