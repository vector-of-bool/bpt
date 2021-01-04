#include "./parallel.hpp"

#include <dds/util/signal.hpp>

#include <dds/util/log.hpp>

using namespace dds;

void dds::log_exception(std::exception_ptr eptr) noexcept {
    try {
        std::rethrow_exception(eptr);
    } catch (const dds::user_cancelled&) {
        // Don't log this one. The user knows what they did
    } catch (const std::exception& e) {
        dds_log(error, "{}", e.what());
    }
}
