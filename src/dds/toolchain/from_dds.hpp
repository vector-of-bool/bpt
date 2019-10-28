#pragma once

#include <dds/toolchain/toolchain.hpp>

#include <libman/parse_fwd.hpp>

#include <string_view>

namespace dds {

class toolchain;

toolchain parse_toolchain_dds(std::string_view str,
                              std::string_view context = "Loading toolchain file");
toolchain parse_toolchain_dds(const lm::pair_list&,
                              std::string_view context = "Loading toolchain file");

}  // namespace dds
