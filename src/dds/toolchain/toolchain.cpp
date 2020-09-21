#include "./toolchain.hpp"

#include <dds/toolchain/from_json.hpp>
#include <dds/toolchain/prep.hpp>
#include <dds/util/algo.hpp>
#include <dds/util/log.hpp>
#include <dds/util/paths.hpp>
#include <dds/util/string.hpp>

#include <range/v3/view/transform.hpp>

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
    ret._c_compile           = prep.c_compile;
    ret._cxx_compile         = prep.cxx_compile;
    ret._inc_template        = prep.include_template;
    ret._extern_inc_template = prep.external_include_template;
    ret._def_template        = prep.define_template;
    ret._link_archive        = prep.link_archive;
    ret._link_exe            = prep.link_exe;
    ret._warning_flags       = prep.warning_flags;
    ret._archive_prefix      = prep.archive_prefix;
    ret._archive_suffix      = prep.archive_suffix;
    ret._object_prefix       = prep.object_prefix;
    ret._object_suffix       = prep.object_suffix;
    ret._exe_prefix          = prep.exe_prefix;
    ret._exe_suffix          = prep.exe_suffix;
    ret._deps_mode           = prep.deps_mode;
    ret._tty_flags           = prep.tty_flags;
    return ret;
}

vector<string> toolchain::include_args(const fs::path& p) const noexcept {
    return replace(_inc_template, "[path]", p.string());
}

vector<string> toolchain::external_include_args(const fs::path& p) const noexcept {
    return replace(_extern_inc_template, "[path]", p.string());
}

vector<string> toolchain::definition_args(std::string_view s) const noexcept {
    return replace(_def_template, "[def]", s);
}

static fs::path shortest_path_from(path_ref file, path_ref base) {
    auto relative = file.lexically_normal().lexically_proximate(base);
    auto abs      = file.lexically_normal();
    if (relative.string().size() > abs.string().size()) {
        return abs;
    } else {
        return relative;
    }
}

template <typename R>
static auto shortest_path_args(path_ref base, R&& r) {
    return ranges::views::all(r)  //
        | ranges::views::transform(
               [base](auto&& path) { return shortest_path_from(path, base).string(); });  //
}

compile_command_info toolchain::create_compile_command(const compile_file_spec& spec,
                                                       path_ref                 cwd,
                                                       toolchain_knobs knobs) const noexcept {
    using namespace std::literals;

    dds_log(trace,
            "Calculate compile command for source file [{}] to object file [{}]",
            spec.source_path.string(),
            spec.out_path.string());

    language lang = spec.lang;
    if (lang == language::automatic) {
        if (spec.source_path.extension() == ".c" || spec.source_path.extension() == ".C") {
            lang = language::c;
        } else {
            lang = language::cxx;
        }
    }

    vector<string> flags;
    if (knobs.is_tty) {
        dds_log(trace, "Enabling TTY flags.");
        extend(flags, _tty_flags);
    }

    dds_log(trace, "#include-search dirs:");
    for (auto&& inc_dir : spec.include_dirs) {
        dds_log(trace, "  - search: {}", inc_dir.string());
        auto shortest = shortest_path_from(inc_dir, cwd);
        auto inc_args = include_args(shortest);
        extend(flags, inc_args);
    }

    for (auto&& ext_inc_dir : spec.external_include_dirs) {
        dds_log(trace, "  - search (external): {}", ext_inc_dir.string());
        auto inc_args = external_include_args(ext_inc_dir);
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

    if (_deps_mode == file_deps_mode::gnu) {
        gnu_depfile_path = spec.out_path;
        gnu_depfile_path->replace_extension(gnu_depfile_path->extension().string() + ".d");
        extend(flags,
               {"-MD"sv,
                "-MF"sv,
                std::string_view(gnu_depfile_path->string()),
                "-MQ"sv,
                std::string_view(spec.out_path.string())});
    } else if (_deps_mode == file_deps_mode::msvc) {
        flags.push_back("/showIncludes");
    }

    vector<string> command;
    auto&          cmd_template = lang == language::c ? _c_compile : _cxx_compile;
    for (auto arg : cmd_template) {
        if (arg == "[flags]") {
            extend(command, flags);
        } else {
            arg = replace(arg, "[in]", spec.source_path.string());
            arg = replace(arg, "[out]", spec.out_path.string());
            command.push_back(arg);
        }
    }
    return {command, gnu_depfile_path};
}

vector<string> toolchain::create_archive_command(const archive_spec& spec,
                                                 path_ref            cwd,
                                                 toolchain_knobs) const noexcept {
    vector<string> cmd;
    dds_log(trace, "Creating archive command [output: {}]", spec.out_path.string());
    auto out_arg = shortest_path_from(spec.out_path, cwd).string();
    for (auto& arg : _link_archive) {
        if (arg == "[in]") {
            dds_log(trace, "Expand [in] placeholder:");
            for (auto&& in : spec.input_files) {
                dds_log(trace, "  - input: [{}]", in.string());
            }
            extend(cmd, shortest_path_args(cwd, spec.input_files));
        } else {
            cmd.push_back(replace(arg, "[out]", out_arg));
        }
    }
    return cmd;
}

vector<string> toolchain::create_link_executable_command(const link_exe_spec& spec,
                                                         path_ref             cwd,
                                                         toolchain_knobs) const noexcept {
    vector<string> cmd;
    dds_log(trace, "Creating link command [output: {}]", spec.output.string());
    for (auto& arg : _link_exe) {
        if (arg == "[in]") {
            dds_log(trace, "Expand [in] placeholder:");
            for (auto&& in : spec.inputs) {
                dds_log(trace, "  - input: [{}]", in.string());
            }
            extend(cmd, shortest_path_args(cwd, spec.inputs));
        } else {
            cmd.push_back(replace(arg, "[out]", shortest_path_from(spec.output, cwd).string()));
        }
    }
    return cmd;
}

std::optional<toolchain> toolchain::get_builtin(std::string_view tc_id) noexcept {
    using namespace std::literals;

    json5::data tc_data  = json5::data::mapping_type();
    auto&       root_map = tc_data.as_object();

    if (starts_with(tc_id, "debug:")) {
        tc_id = tc_id.substr("debug:"sv.length());
        root_map.emplace("debug", true);
    }

    if (starts_with(tc_id, "ccache:")) {
        tc_id = tc_id.substr("ccache:"sv.length());
        root_map.emplace("compiler_launcher", "ccache");
    }

#define CXX_VER_TAG(str, version)                                                                  \
    if (starts_with(tc_id, str)) {                                                                 \
        tc_id = tc_id.substr(std::string_view(str).length());                                      \
        root_map.emplace("cxx_version", version);                                                  \
    }                                                                                              \
    static_assert(true)

    CXX_VER_TAG("c++98:", "c++98");
    CXX_VER_TAG("c++03:", "c++03");
    CXX_VER_TAG("c++11:", "c++11");
    CXX_VER_TAG("c++14:", "c++14");
    CXX_VER_TAG("c++17:", "c++17");
    CXX_VER_TAG("c++20:", "c++20");

    struct compiler_info {
        string c;
        string cxx;
        string id;
    };

    auto opt_triple = [&]() -> std::optional<compiler_info> {
        if (starts_with(tc_id, "gcc") || starts_with(tc_id, "clang")) {
            const bool is_gcc   = starts_with(tc_id, "gcc");
            const bool is_clang = starts_with(tc_id, "clang");

            const auto [c_compiler_base, cxx_compiler_base, compiler_id] = [&]() -> compiler_info {
                if (is_gcc) {
                    return {"gcc", "g++", "gnu"};
                } else if (is_clang) {
                    return {"clang", "clang++", "clang"};
                }
                assert(false && "Unreachable");
                std::terminate();
            }();

            const auto compiler_suffix = [&]() -> std::string {
                if (ends_with(tc_id, "-7")) {
                    return "-7";
                } else if (ends_with(tc_id, "-8")) {
                    return "-8";
                } else if (ends_with(tc_id, "-9")) {
                    return "-9";
                } else if (ends_with(tc_id, "-10")) {
                    return "-10";
                } else if (ends_with(tc_id, "-11")) {
                    return "-11";
                } else if (ends_with(tc_id, "-12")) {
                    return "-12";
                } else if (ends_with(tc_id, "-13")) {
                    return "-13";
                }
                return "";
            }();

            auto c_compiler_name = c_compiler_base + compiler_suffix;
            if (c_compiler_name != tc_id) {
                return std::nullopt;
            }
            auto cxx_compiler_name = cxx_compiler_base + compiler_suffix;
            return compiler_info{c_compiler_name, cxx_compiler_name, compiler_id};
        } else if (tc_id == "msvc") {
            return compiler_info{"cl.exe", "cl.exe", "msvc"};
        } else {
            return std::nullopt;
        }
    }();

    if (!opt_triple) {
        return std::nullopt;
    }

    if (starts_with(tc_id, "gcc") || starts_with(tc_id, "clang")) {
        json5::data& arr = root_map.emplace("link_flags", json5::data::array_type()).first->second;
        arr.as_array().emplace_back("-static-libgcc");
        arr.as_array().emplace_back("-static-libstdc++");
    }

    root_map.emplace("c_compiler", opt_triple->c);
    root_map.emplace("cxx_compiler", opt_triple->cxx);
    root_map.emplace("compiler_id", opt_triple->id);
    return parse_toolchain_json_data(tc_data);
}

std::optional<dds::toolchain> dds::toolchain::get_default() {
    auto candidates = {
        fs::current_path() / "toolchain.json5",
        fs::current_path() / "toolchain.jsonc",
        fs::current_path() / "toolchain.json",
        dds_config_dir() / "toolchain.json5",
        dds_config_dir() / "toolchain.jsonc",
        dds_config_dir() / "toolchain.json",
        user_home_dir() / "toolchain.json5",
        user_home_dir() / "toolchain.jsonc",
        user_home_dir() / "toolchain.json",
    };
    for (auto&& cand : candidates) {
        dds_log(trace, "Checking for default toolchain at [{}]", cand.string());
        if (fs::exists(cand)) {
            dds_log(debug, "Using default toolchain file: {}", cand.string());
            return parse_toolchain_json5(slurp_file(cand));
        }
    }
    return std::nullopt;
}
