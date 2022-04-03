#pragma once

namespace sbs {

/**
 * @brief Error object telling the top-level error handler to exit normally with the given exit code
 *
 */
struct e_exit {
    int value;
};

/**
 * @brief Throw an exceptoin with an e_exit for the given exit code.
 */
[[noreturn]] void throw_system_exit(int value);

}  // namespace sbs