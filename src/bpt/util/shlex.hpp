#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace dds {

std::vector<std::string> split_shell_string(std::string_view s);

}  // namespace dds