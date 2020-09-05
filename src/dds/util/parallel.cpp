#include "./parallel.hpp"

#include <dds/util/log.hpp>

using namespace dds;

void dds::log_exception(std::exception_ptr eptr) noexcept {
    try {
        std::rethrow_exception(eptr);
    } catch (const std::exception& e) {
        dds_log(error, e.what());
    }
}
