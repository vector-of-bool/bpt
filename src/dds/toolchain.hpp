#ifndef DDS_TOOLCHAIN_HPP_INCLUDED
#define DDS_TOOLCHAIN_HPP_INCLUDED

#include <dds/util.hpp>

#include <optional>
#include <string>
#include <vector>

namespace dds {

std::vector<std::string> split_shell_string(std::string_view s);

enum class language {
    automatic,
    c,
    cxx,
};

struct compile_file_spec {
    fs::path                 source_path;
    fs::path                 out_path;
    std::vector<std::string> definitions     = {};
    std::vector<fs::path>    include_dirs    = {};
    language                 lang            = language::automatic;
    bool                     enable_warnings = false;
};

struct archive_spec {
    std::vector<fs::path> input_files;
    fs::path              out_path;
};

struct link_exe_spec {
    std::vector<fs::path> inputs;
    fs::path              output;
};

class toolchain {
    using string_seq = std::vector<std::string>;

    string_seq  _c_compile;
    string_seq  _cxx_compile;
    string_seq  _inc_template;
    string_seq  _def_template;
    string_seq  _archive_template;
    string_seq  _link_exe_template;
    string_seq  _warning_flags;
    std::string _archive_suffix;
    std::string _object_suffix;
    std::string _exe_suffix;

public:
    toolchain() = default;

    toolchain(std::string_view c_compile,
              std::string_view cxx_compile,
              std::string_view inc_template,
              std::string_view def_template,
              std::string_view archive_template,
              std::string_view link_exe_template,
              std::string_view warning_flags,
              std::string_view archive_suffix,
              std::string_view object_suffix,
              std::string_view exe_suffix)
        : _c_compile(split_shell_string(c_compile))
        , _cxx_compile(split_shell_string(cxx_compile))
        , _inc_template(split_shell_string(inc_template))
        , _def_template(split_shell_string(def_template))
        , _archive_template(split_shell_string(archive_template))
        , _link_exe_template(split_shell_string(link_exe_template))
        , _warning_flags(split_shell_string(warning_flags))
        , _archive_suffix(archive_suffix)
        , _object_suffix(object_suffix)
        , _exe_suffix(exe_suffix) {}

    static toolchain load_from_file(fs::path);

    auto& archive_suffix() const noexcept { return _archive_suffix; }
    auto& object_suffix() const noexcept { return _object_suffix; }
    auto& executable_suffix() const noexcept { return _exe_suffix; }

    std::vector<std::string> definition_args(std::string_view s) const noexcept;
    std::vector<std::string> include_args(const fs::path& p) const noexcept;
    std::vector<std::string> create_compile_command(const compile_file_spec&) const noexcept;
    std::vector<std::string> create_archive_command(const archive_spec&) const noexcept;
    std::vector<std::string> create_link_executable_command(const link_exe_spec&) const noexcept;

    static std::optional<toolchain> get_builtin(std::string_view key) noexcept;
};

}  // namespace dds

#endif  // DDS_TOOLCHAIN_HPP_INCLUDED