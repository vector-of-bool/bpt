#pragma once

#include <bpt/toolchain/toolchain.hpp>

#include <json5/data.hpp>

#include <string_view>

namespace dds {

toolchain parse_toolchain_json5(std::string_view json5,
                                std::string_view context = "Loading toolchain JSON");

toolchain parse_toolchain_json_data(const json5::data& data,
                                    std::string_view   context = "Loading toolchain JSON");

}  // namespace dds