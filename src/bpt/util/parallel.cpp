#include "./parallel.hpp"

#include <bpt/util/signal.hpp>

#include <bpt/util/log.hpp>

using namespace bpt;

void bpt::log_exception(std::exception_ptr eptr) noexcept {
    try {
        std::rethrow_exception(eptr);
    } catch (const bpt::user_cancelled&) {
        // Don't log this one. The user knows what they did
    } catch (const std::exception& e) {
        bpt_log(error, "{}", e.what());
    }
}
