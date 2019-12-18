#pragma once

#include <spdlog/fmt/fmt.h>

#include <stdexcept>
#include <string_view>

namespace dds {

struct exception_base : std::runtime_error {
    using runtime_error::runtime_error;
};

struct user_error_base : exception_base {
    using exception_base::exception_base;

    virtual std::string      error_reference() const noexcept = 0;
    virtual std::string_view explanation() const noexcept     = 0;
};

enum class errc {
    none = 0,
    invalid_builtin_toolchain,
    no_such_catalog_package,
};

std::string      error_reference_of(errc) noexcept;
std::string_view explanation_of(errc) noexcept;

template <errc ErrorCode>
struct user_error : user_error_base {
    using user_error_base::user_error_base;

    std::string error_reference() const noexcept override { return error_reference_of(ErrorCode); }
    std::string_view explanation() const noexcept override { return explanation_of(ErrorCode); }
};

using error_invalid_default_toolchain = user_error<errc::invalid_builtin_toolchain>;

template <errc ErrorCode, typename... Args>
[[noreturn]] void throw_user_error(std::string_view fmt_str, Args&&... args) {
    throw user_error<ErrorCode>(fmt::format(fmt_str, std::forward<Args>(args)...));
}

}  // namespace dds
