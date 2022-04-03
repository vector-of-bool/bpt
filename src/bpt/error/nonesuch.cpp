#include "./nonesuch.hpp"

#include <bpt/util/log.hpp>

#include <fansi/styled.hpp>

using namespace dds;
using namespace fansi::literals;

void e_nonesuch::log_error(std::string_view fmt) const noexcept {
    dds_log(error, fmt, given);
    if (nearest) {
        dds_log(error, "  (Did you mean '.br.yellow[{}]'?)"_styled, *nearest);
    }
}
