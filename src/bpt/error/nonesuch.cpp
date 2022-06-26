#include "./nonesuch.hpp"

#include <bpt/util/log.hpp>

#include <fansi/styled.hpp>

#include <iomanip>

using namespace bpt;
using namespace fansi::literals;

void e_nonesuch::log_error(std::string_view fmt) const noexcept {
    bpt_log(error, fmt, given);
    if (nearest) {
        bpt_log(error, "  (Did you mean '.br.yellow[{}]'?)"_styled, *nearest);
    }
}

void bpt::e_nonesuch::ostream_into(std::ostream& out) const noexcept {
    out << "bpt::e_nonsuch: Given " << std::quoted(given);
    if (nearest.has_value()) {
        out << " (nearest is " << std::quoted(*nearest) << ")";
    }
}