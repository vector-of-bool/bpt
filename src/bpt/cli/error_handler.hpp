#pragma once

#include <functional>

namespace dds {

int handle_cli_errors(std::function<int()>) noexcept;

}  // namespace dds
