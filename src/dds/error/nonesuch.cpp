#include "./nonesuch.hpp"

#include <dds/util/log.hpp>

using namespace dds;

void e_nonesuch::log_error(std::string_view fmt) const noexcept {
    dds_log(error, fmt, given);
    if (nearest) {
        dds_log(error, "  (Did you mean '{}'?)", *nearest);
    }
}
