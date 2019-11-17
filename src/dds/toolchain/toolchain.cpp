#include "./toolchain.hpp"

#include <dds/toolchain/prep.hpp>
#include <dds/util/algo.hpp>
#include <dds/util/string.hpp>

#include <cassert>
#include <optional>
#include <string>
#include <vector>

using namespace dds;

using std::optional;
using std::string;
using std::string_view;
using std::vector;
using opt_string = optional<string>;

toolchain toolchain::realize(const toolchain_prep& prep) {
    toolchain ret;
    ret._c_compile      = prep.c_compile;
    ret._cxx_compile    = prep.cxx_compile;
    ret._inc_template   = prep.include_template;
    ret._def_template   = prep.define_template;
    ret._link_archive   = prep.link_archive;
    ret._link_exe       = prep.link_exe;
    ret._warning_flags  = prep.warning_flags;
    ret._archive_prefix = prep.archive_prefix;
    ret._archive_suffix = prep.archive_suffix;
    ret._object_prefix  = prep.object_prefix;
    ret._object_suffix  = prep.object_suffix;
    ret._exe_prefix     = prep.exe_prefix;
    ret._exe_suffix     = prep.exe_suffix;
    ret._deps_mode      = prep.deps_mode;
    return ret;
}

vector<string> dds::split_shell_string(std::string_view shell) {
    char cur_quote  = 0;
    bool is_escaped = false;

    vector<string> acc;

    const auto begin = shell.begin();
    auto       iter  = begin;
    const auto end   = shell.end();

    opt_string token;
    while (iter != end) {
        const char c = *iter++;
        if (is_escaped) {
            if (c == '\n') {
                // Ignore the newline
            } else if (cur_quote || c != cur_quote || c == '\\') {
                // Escaped `\` character
                token = token.value_or("") + c;
            } else {
                // Regular escape sequence
                token = token.value_or("") + '\\' + c;
            }
            is_escaped = false;
        } else if (c == '\\') {
            is_escaped = true;
        } else if (cur_quote) {
            if (c == cur_quote) {
                // End of quoted token;
                cur_quote = 0;
            } else {
                token = token.value_or("") + c;
            }
        } else if (c == '"' || c == '\'') {
            // Beginning of a quoted token
            cur_quote = c;
            token     = "";
        } else if (c == '\t' || c == ' ' || c == '\n' || c == '\r' || c == '\f') {
            // We've reached unquoted whitespace
            if (token.has_value()) {
                acc.push_back(move(*token));
            }
            token.reset();
        } else {
            // Just a regular character
            token = token.value_or("") + c;
        }
    }

    if (token.has_value()) {
        acc.push_back(move(*token));
    }

    return acc;
}

vector<string> toolchain::include_args(const fs::path& p) const noexcept {
    return replace(_inc_template, "<PATH>", p.string());
}

vector<string> toolchain::definition_args(std::string_view s) const noexcept {
    return replace(_def_template, "<DEF>", s);
}

compile_command_info toolchain::create_compile_command(const compile_file_spec& spec) const
    noexcept {
    vector<string> flags;

    using namespace std::literals;

    language lang = spec.lang;
    if (lang == language::automatic) {
        if (spec.source_path.extension() == ".c" || spec.source_path.extension() == ".C") {
            lang = language::c;
        } else {
            lang = language::cxx;
        }
    }

    auto& cmd_template = lang == language::c ? _c_compile : _cxx_compile;

    for (auto&& inc_dir : spec.include_dirs) {
        auto inc_args = include_args(inc_dir);
        extend(flags, inc_args);
    }

    for (auto&& def : spec.definitions) {
        auto def_args = definition_args(def);
        extend(flags, def_args);
    }

    if (spec.enable_warnings) {
        extend(flags, _warning_flags);
    }

    std::optional<fs::path> gnu_depfile_path;

    if (_deps_mode == deps_mode::gnu) {
        gnu_depfile_path = spec.out_path;
        gnu_depfile_path->replace_extension(gnu_depfile_path->extension().string() + ".d");
        extend(flags, {"-MD"sv, "-MF"sv, std::string_view(gnu_depfile_path->string())});
    } else if (_deps_mode == deps_mode::msvc) {
        flags.push_back("/showIncludes");
    }

    vector<string> command;
    for (auto arg : cmd_template) {
        if (arg == "<FLAGS>") {
            extend(command, flags);
        } else {
            arg = replace(arg, "<IN>", spec.source_path.string());
            arg = replace(arg, "<OUT>", spec.out_path.string());
            command.push_back(arg);
        }
    }
    return {command, gnu_depfile_path};
}

vector<string> toolchain::create_archive_command(const archive_spec& spec) const noexcept {
    vector<string> cmd;
    for (auto& arg : _link_archive) {
        if (arg == "<IN>") {
            std::transform(spec.input_files.begin(),
                           spec.input_files.end(),
                           std::back_inserter(cmd),
                           [](auto&& p) { return p.string(); });
        } else {
            cmd.push_back(replace(arg, "<OUT>", spec.out_path.string()));
        }
    }
    return cmd;
}

vector<string> toolchain::create_link_executable_command(const link_exe_spec& spec) const noexcept {
    vector<string> cmd;
    for (auto& arg : _link_exe) {
        if (arg == "<IN>") {
            std::transform(spec.inputs.begin(),
                           spec.inputs.end(),
                           std::back_inserter(cmd),
                           [](auto&& p) { return p.string(); });
        } else {
            cmd.push_back(replace(arg, "<OUT>", spec.output.string()));
        }
    }
    return cmd;
}

std::optional<toolchain> toolchain::get_builtin(std::string_view s) noexcept {
    toolchain ret;

    using namespace std::literals;

    if (starts_with(s, "ccache:")) {
        s = s.substr("ccache:"sv.length());
        ret._c_compile.push_back("ccache");
        ret._cxx_compile.push_back("ccache");
    }

    if (starts_with(s, "gcc") || starts_with(s, "clang")) {
        ret._inc_template   = {"-isystem", "<PATH>"};
        ret._def_template   = {"-D", "<DEF>"};
        ret._archive_suffix = ".a";
        ret._object_suffix  = ".o";
        ret._warning_flags  = {"-Wall", "-Wextra"};
        ret._link_archive   = {"ar", "rcs", "<OUT>", "<IN>"};

        std::vector<std::string> common_flags = {
            "<FLAGS>",
            "-g",
            "-fPIC",
            "-fdiagnostics-color",
            "-pthread",
            "-c",
            "-o",
            "<OUT>",
            "<IN>",
        };
        std::vector<std::string> c_flags;
        std::vector<std::string> cxx_flags = {"-std=c++17"};

        std::string c_compiler_base;
        std::string cxx_compiler_base;
        std::string compiler_suffix;

        if (starts_with(s, "gcc")) {
            c_compiler_base   = "gcc";
            cxx_compiler_base = "g++";
            common_flags.push_back("-O0");
        } else if (starts_with(s, "clang")) {
            c_compiler_base   = "clang";
            cxx_compiler_base = "clang++";
        } else {
            assert(false && "Unreachable");
            std::terminate();
        }

        if (ends_with(s, "-7")) {
            compiler_suffix = "-7";
        } else if (ends_with(s, "-8")) {
            compiler_suffix = "-8";
        } else if (ends_with(s, "-9")) {
            compiler_suffix = "-9";
        } else if (ends_with(s, "-10")) {
            compiler_suffix = "-10";
        }

        auto c_compiler_name = c_compiler_base + compiler_suffix;
        if (c_compiler_name != s) {
            return std::nullopt;
        }
        auto cxx_compiler_name = cxx_compiler_base + compiler_suffix;

        ret._c_compile.push_back(c_compiler_name);
        extend(ret._c_compile, common_flags);
        extend(ret._c_compile, c_flags);

        ret._cxx_compile.push_back(cxx_compiler_name);
        extend(ret._cxx_compile, common_flags);
        extend(ret._cxx_compile, cxx_flags);

        ret._link_exe.push_back(cxx_compiler_name);
        extend(ret._link_exe,
               {
                   "-g",
                   "-fPIC",
                   "-fdiagnostics-color",
                   "<IN>",
                   "-pthread",
                   "-lstdc++fs",
                   "-o",
                   "<OUT>",
               });
    } else if (s == "msvc") {
        ret._inc_template = {"/I<PATH>"};
        ret._def_template = {"/D<DEF>"};
        ret._c_compile    = {"cl.exe", "/nologo", "<FLAGS>", "/c", "<IN>", "/Fo<OUT>"};
        ret._cxx_compile  = {"cl.exe",
                            "/nologo",
                            "<FLAGS>",
                            "/std:c++latest",
                            "/permissive-",
                            "/EHsc",
                            "/c",
                            "<IN>",
                            "/Fo<OUT>"};
        std::vector<std::string_view> common_flags = {"/Z7", "/O2", "/MT", "/DEBUG"};
        extend(ret._c_compile, common_flags);
        extend(ret._cxx_compile, common_flags);
        ret._archive_suffix = ".lib";
        ret._object_suffix  = ".obj";
        ret._exe_suffix     = ".exe";
        ret._link_archive   = {"lib", "/nologo", "/OUT:<OUT>", "<IN>"};
        ret._link_exe       = {"cl.exe", "/nologo", "/std:c++latest", "/EHsc", "<IN>", "/Fe<OUT>"};
        ret._warning_flags  = {"/W4"};
    } else {
        return std::nullopt;
    }

    return ret;
}
