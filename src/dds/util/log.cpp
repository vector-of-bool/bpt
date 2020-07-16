#include "./log.hpp"

#include <neo/assert.hpp>

#include <spdlog/spdlog.h>

void dds::log::log_print(dds::log::level l, std::string_view msg) noexcept {
    static auto logger = [] {
        auto logger = spdlog::default_logger_raw();
        logger->set_level(spdlog::level::trace);
        return logger;
    }();

    const auto lvl = [&] {
        switch (l) {
        case level::trace:
            return spdlog::level::trace;
        case level::debug:
            return spdlog::level::debug;
        case level::info:
            return spdlog::level::info;
        case level::warn:
            return spdlog::level::warn;
        case level::error:
            return spdlog::level::err;
        case level::critical:
            return spdlog::level::critical;
        }
        neo_assert_always(invariant, false, "Invalid log level", msg, int(l));
    }();

    logger->log(lvl, msg);
}
