#pragma once

#include <dds/build/file_deps.hpp>

#include <string>
#include <vector>

namespace dds {

class toolchain;

struct toolchain_prep {
    using string_seq = std::vector<std::string>;
    string_seq c_compile;
    string_seq cxx_compile;
    string_seq include_template;
    string_seq external_include_template;
    string_seq define_template;
    string_seq link_archive;
    string_seq link_exe;
    string_seq warning_flags;
    string_seq tty_flags;

    // Environment variables that should be taken into consideration when generating a toolchain ID
    string_seq consider_envs;

    string_seq c_source_type_flags;
    string_seq cxx_source_type_flags;
    string_seq syntax_only_flags;

    std::string archive_prefix;
    std::string archive_suffix;
    std::string object_prefix;
    std::string object_suffix;
    std::string exe_prefix;
    std::string exe_suffix;

    enum file_deps_mode deps_mode;

    [[nodiscard]] toolchain realize() const;

    [[nodiscard]] std::uint64_t compute_hash() const noexcept;
};

}  // namespace dds
