#include "./log.hpp"

#include <neo/assert.hpp>

#include <spdlog/spdlog.h>

#include <ostream>

#if _WIN32
#include <consoleapi2.h>
static void set_utf8_output() {
    // 65'001 is the codepage id for UTF-8 output
    ::SetConsoleOutputCP(65'001);
}
#else
static void set_utf8_output() {
    // Nothing on other platforms
}
#endif

void dds::log::log_print(dds::log::level l, std::string_view msg) noexcept {
    static auto logger_inst = [] {
        auto logger = spdlog::default_logger_raw();
        logger->set_level(spdlog::level::trace);
        set_utf8_output();
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

    logger_inst->log(lvl, "{}", msg);
}
