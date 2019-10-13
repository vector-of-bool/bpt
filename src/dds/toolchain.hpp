#ifndef DDS_TOOLCHAIN_HPP_INCLUDED
#define DDS_TOOLCHAIN_HPP_INCLUDED

#include <dds/util.hpp>

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

class toolchain {
    using string_seq = std::vector<std::string>;

    string_seq  _c_compile;
    string_seq  _cxx_compile;
    string_seq  _inc_template;
    string_seq  _def_template;
    string_seq  _archive_template;
    std::string _archive_suffix;
    string_seq  _warning_flags;

public:
    toolchain(const std::string& c_compile,
              const std::string& cxx_compile,
              const std::string& inc_template,
              const std::string& def_template,
              const std::string& archive_template,
              const std::string& archive_suffix,
              const std::string& warning_flags)
        : _c_compile(split_shell_string(c_compile))
        , _cxx_compile(split_shell_string(cxx_compile))
        , _inc_template(split_shell_string(inc_template))
        , _def_template(split_shell_string(def_template))
        , _archive_template(split_shell_string(archive_template))
        , _archive_suffix(archive_suffix)
        , _warning_flags(split_shell_string(warning_flags)) {}

    static toolchain load_from_file(fs::path);

    auto& archive_suffix() const noexcept { return _archive_suffix; }

    std::vector<std::string> definition_args(std::string_view s) const noexcept;
    std::vector<std::string> include_args(const fs::path& p) const noexcept;
    std::vector<std::string> create_compile_command(const compile_file_spec&) const noexcept;
    std::vector<std::string> create_archive_command(const archive_spec&) const noexcept;
};

}  // namespace dds

#endif  // DDS_TOOLCHAIN_HPP_INCLUDED