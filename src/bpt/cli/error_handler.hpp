#pragma once

#include <functional>

namespace bpt {

int handle_cli_errors(std::function<int()>) noexcept;

}  // namespace bpt
