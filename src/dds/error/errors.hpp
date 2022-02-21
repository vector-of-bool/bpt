#pragma once

#include <fmt/core.h>

#include <stdexcept>
#include <string_view>

namespace dds {

enum class errc {
    none = 0,
    invalid_builtin_toolchain,
    no_default_toolchain,
    test_failure,
    compile_failure,
    archive_failure,
    link_failure,

    invalid_remote_url,
    sdist_exists,

    cyclic_usage,

    invalid_pkg_filesystem,
};

std::string      error_reference_of(errc) noexcept;
std::string_view explanation_of(errc) noexcept;
std::string_view default_error_string(errc) noexcept;
struct exception_base : std::runtime_error {
    using runtime_error::runtime_error;
};

struct error_base : exception_base {
    using exception_base::exception_base;

    virtual errc     get_errc() const noexcept = 0;
    std::string      error_reference() const noexcept { return error_reference_of(get_errc()); }
    std::string_view explanation() const noexcept { return explanation_of(get_errc()); }
};

struct external_error_base : error_base {
    using error_base::error_base;
};

struct user_error_base : error_base {
    using error_base::error_base;
};

template <errc ErrorCode>
struct user_error : user_error_base {
    using user_error_base::user_error_base;
    errc get_errc() const noexcept override { return ErrorCode; }
};

template <errc ErrorCode>
struct external_error : external_error_base {
    using external_error_base::external_error_base;
    errc get_errc() const noexcept override { return ErrorCode; }
};

using error_invalid_default_toolchain = user_error<errc::invalid_builtin_toolchain>;

template <errc ErrorCode, typename... Args>
auto make_user_error(std::string_view fmt_str, Args&&... args) {
    return user_error<ErrorCode>(fmt::format(fmt_str, std::forward<Args>(args)...));
}

template <errc ErrorCode>
auto make_user_error() {
    return user_error<ErrorCode>(std::string(default_error_string(ErrorCode)));
}

template <errc ErrorCode, typename... Args>
[[noreturn]] void throw_user_error(std::string_view fmt_str, Args&&... args) {
    throw user_error<ErrorCode>(fmt::format(fmt_str, std::forward<Args>(args)...));
}

template <errc ErrorCode>
[[noreturn]] void throw_user_error() {
    throw user_error<ErrorCode>(std::string(default_error_string(ErrorCode)));
}

template <errc ErrorCode, typename... Args>
auto make_external_error(std::string_view fmt_str, Args&&... args) {
    return external_error<ErrorCode>(fmt::format(fmt_str, std::forward<Args>(args)...));
}

template <errc ErrorCode>
auto make_external_error() {
    return external_error<ErrorCode>(std::string(default_error_string(ErrorCode)));
}

template <errc ErrorCode, typename... Args>
[[noreturn]] void throw_external_error(std::string_view fmt_str, Args&&... args) {
    throw make_external_error<ErrorCode>(fmt_str, std::forward<Args>(args)...);
}

template <errc ErrorCode>
[[noreturn]] void throw_external_error() {
    throw make_external_error<ErrorCode>(std::string(default_error_string(ErrorCode)));
}

}  // namespace dds
