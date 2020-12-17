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
    silent,
};

inline level current_log_level = level::info;

void log_print(level l, std::string_view s) noexcept;

void init_logger() noexcept;

template <typename T>
concept formattable = requires(const T item) {
    fmt::format("{}", item);
};

inline bool level_enabled(level l) { return int(l) >= int(current_log_level); }

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

#define dds_log(Level, str, ...)                                                                   \
    do {                                                                                           \
        if (int(dds::log::level::Level) >= int(dds::log::current_log_level)) {                     \
            ::dds::log::log(::dds::log::level::Level, str __VA_OPT__(, ) __VA_ARGS__);             \
        }                                                                                          \
    } while (0)

}  // namespace dds::log