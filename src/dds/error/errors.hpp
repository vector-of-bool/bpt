#pragma once

#include <fmt/core.h>

#include <stdexcept>
#include <string_view>

namespace dds {

enum class errc {
    none = 0,
    invalid_builtin_toolchain,
    no_default_toolchain,
    no_such_catalog_package,
    git_url_ref_mutual_req,
    test_failure,
    compile_failure,
    archive_failure,
    link_failure,

    catalog_too_new,
    corrupted_catalog_db,
    invalid_catalog_json,
    no_catalog_remote_info,

    git_clone_failure,
    invalid_remote_url,
    http_download_failure,
    invalid_repo_transform,
    sdist_ident_mismatch,
    sdist_exists,

    corrupted_build_db,

    invalid_lib_manifest,
    invalid_pkg_manifest,
    invalid_version_range_string,
    invalid_version_string,
    invalid_pkg_id,
    invalid_pkg_name,
    unknown_test_driver,
    dependency_resolve_failure,
    dup_lib_name,
    unknown_usage_name,

    invalid_lib_filesystem,
    invalid_pkg_filesystem,

    template_error,
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
[[noreturn]] void throw_user_error(std::string_view fmt_str, Args&&... args) {
    throw user_error<ErrorCode>(fmt::format(fmt_str, std::forward<Args>(args)...));
}

template <errc ErrorCode>
[[noreturn]] void throw_user_error() {
    throw user_error<ErrorCode>(std::string(default_error_string(ErrorCode)));
}

template <errc ErrorCode, typename... Args>
[[noreturn]] void throw_external_error(std::string_view fmt_str, Args&&... args) {
    throw external_error<ErrorCode>(fmt::format(fmt_str, std::forward<Args>(args)...));
}

template <errc ErrorCode>
[[noreturn]] void throw_external_error() {
    throw external_error<ErrorCode>(std::string(default_error_string(ErrorCode)));
}

}  // namespace dds
