#include "./parallel.hpp"

#include <spdlog/spdlog.h>

using namespace dds;

void dds::log_exception(std::exception_ptr eptr) noexcept {
    try {
        std::rethrow_exception(eptr);
    } catch (const std::exception& e) {
        spdlog::error(e.what());
    }
}
