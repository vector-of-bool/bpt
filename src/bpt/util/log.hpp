#pragma once

#include <fmt/core.h>

#include <string_view>

namespace bpt::log {

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

struct ev_log {
    log::level       level;
    std::string_view message;

    void print() const noexcept;
};

void log_print(level l, std::string_view s) noexcept;
void log_emit(ev_log) noexcept;

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
        log_emit(ev_log{l, message});
    }
}

template <formattable... Args>
void trace(std::string_view s, const Args&... args) {
    log(level::trace, s, args...);
}

#define bpt_log(Level, str, ...)                                                                   \
    do {                                                                                           \
        if (int(bpt::log::level::Level) >= int(bpt::log::current_log_level)) {                     \
            ::bpt::log::log(::bpt::log::level::Level, str __VA_OPT__(, ) __VA_ARGS__);             \
        }                                                                                          \
    } while (0)

}  // namespace bpt::log