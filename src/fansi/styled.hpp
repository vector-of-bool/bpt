#pragma once

#include <cinttypes>
#include <string>
#include <string_view>

namespace fansi {

struct ev_warning {
    std::string_view message;
};

enum class should_style {
    detect,
    force,
    never,
};

std::string stylize(std::string_view text, should_style = should_style::detect);

namespace detail {
const std::string& cached_rendering(const char* ptr) noexcept;
}

inline namespace literals {
inline namespace styled_literals {
inline const std::string& operator""_styled(const char* str, std::size_t) {
    return detail::cached_rendering(str);
}

}  // namespace styled_literals
}  // namespace literals

}  // namespace fansi
