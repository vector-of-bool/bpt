#pragma once

#include <fmt/core.h>

#include <string_view>

namespace dds::log {

enum class level : int {
    trace,
    debug,
    info,
    warn,
    error,
    critical,
};

inline level current_log_level = level::info;

void log_print(level l, std::string_view s) noexcept;

// clang-format off
template <typename T>
concept formattable = requires (const T item) {
    fmt::format("{}", item);
};

template <formattable... Args>
void log(level l, std::string_view s, const Args&... args) noexcept {
    if (int(l) >= int(current_log_level)) {
        auto message = fmt::format(s, args...);
        log_print(l, message);
    }
}

template <formattable... Args>
void trace(std::string_view s, const Args&... args) {
    log(level::trace, s, args...);
}

template <formattable... Args>
void debug(std::string_view s, const Args&... args) {
    log(level::debug, s, args...);
}

template <formattable... Args>
void info(std::string_view s, const Args&... args) {
    log(level::info, s, args...);
}

template <formattable... Args>
void warn(std::string_view s, const Args&... args) {
    log(level::warn, s, args...);
}

template <formattable... Args>
void error(std::string_view s, const Args&... args) {
    log(level::error, s, args...);
}

template <formattable... Args>
void critical(std::string_view s, const Args&&... args) {
    log(level::critical, s, args...);
}

// clang-format on

}  // namespace dds::log