#include "./toolchain.hpp"

#include "./errors.hpp"

#include <bpt/error/doc_ref.hpp>
#include <bpt/error/human.hpp>
#include <bpt/error/marker.hpp>
#include <bpt/error/on_error.hpp>
#include <bpt/toolchain/from_json.hpp>
#include <bpt/toolchain/prep.hpp>
#include <bpt/util/algo.hpp>
#include <bpt/util/fs/io.hpp>
#include <bpt/util/log.hpp>
#include <bpt/util/paths.hpp>
#include <bpt/util/string.hpp>
#include <bpt/util/yaml/convert.hpp>
#include <bpt/util/yaml/parse.hpp>

#include <boost/leaf/exception.hpp>
#include <fmt/ostream.h>
#include <range/v3/view/cartesian_product.hpp>
#include <range/v3/view/transform.hpp>

#include <cassert>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

using namespace bpt;

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

    ret._c_source_type_flags   = prep.c_source_type_flags;
    ret._cxx_source_type_flags = prep.cxx_source_type_flags;
    ret._syntax_only_flags     = prep.syntax_only_flags;

    ret._hash = prep.compute_hash();

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

    fs::path in_file = spec.source_path;

    bpt_log(trace,
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
        bpt_log(trace, "Enabling TTY flags.");
        extend(flags, _tty_flags);
    }

    if (knobs.cache_buster) {
        // This is simply a CPP definition that is used to "bust" any caches that rely on inspecting
        // the command-line of the compiler (including our own).
        auto def = replace(_def_template, "[def]", "__bpt_cachebust=" + *knobs.cache_buster);
        extend(flags, def);
    }

    if (spec.syntax_only) {
        bpt_log(trace, "Enabling syntax-only mode");
        extend(flags, _syntax_only_flags);
        extend(flags, lang == language::c ? _c_source_type_flags : _cxx_source_type_flags);

        in_file = spec.out_path.parent_path() / spec.source_path.filename();
        in_file += ".syncheck";
        bpt_log(trace, "Syntax check file: {}", in_file);

        fs::create_directories(in_file.parent_path());
        auto abs_path = bpt::resolve_path_weak(spec.source_path);
        bpt::write_file(in_file, fmt::format("#include \"{}\"", abs_path.string()));
    }

    bpt_log(trace, "#include search-dirs:");
    for (auto&& inc_dir : spec.include_dirs) {
        bpt_log(trace, "  - search: {}", inc_dir.string());
        auto shortest = shortest_path_from(inc_dir, cwd);
        auto inc_args = include_args(shortest);
        extend(flags, inc_args);
    }

    for (auto&& ext_inc_dir : spec.external_include_dirs) {
        bpt_log(trace, "  - search (external): {}", ext_inc_dir.string());
        auto inc_args = external_include_args(ext_inc_dir);
        extend(flags, inc_args);
    }

    if (knobs.tweaks_dir) {
        bpt_log(trace, "  - search (tweaks): {}", knobs.tweaks_dir->string());
        auto shortest       = shortest_path_from(*knobs.tweaks_dir, cwd);
        auto tweak_inc_args = include_args(shortest);
        extend(flags, tweak_inc_args);
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
            arg = replace(arg, "[in]", in_file.string());
            arg = replace(arg, "[out]", spec.out_path.string());
            command.push_back(arg);
        }
    }
    return {std::move(command), std::move(gnu_depfile_path)};
}

vector<string> toolchain::create_archive_command(const archive_spec& spec,
                                                 path_ref            cwd,
                                                 toolchain_knobs) const noexcept {
    vector<string> cmd;
    bpt_log(trace, "Creating archive command [output: {}]", spec.out_path.string());
    auto out_arg = shortest_path_from(spec.out_path, cwd).string();
    for (auto& arg : _link_archive) {
        if (arg == "[in]") {
            bpt_log(trace, "Expand [in] placeholder:");
            for (auto&& in : spec.input_files) {
                bpt_log(trace, "  - input: [{}]", in.string());
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
    bpt_log(trace, "Creating link command [output: {}]", spec.output.string());
    for (auto& arg : _link_exe) {
        if (arg == "[in]") {
            bpt_log(trace, "Expand [in] placeholder:");
            for (auto&& in : spec.inputs) {
                bpt_log(trace, "  - input: [{}]", in.string());
            }
            extend(cmd, shortest_path_args(cwd, spec.inputs));
        } else {
            cmd.push_back(replace(arg, "[out]", shortest_path_from(spec.output, cwd).string()));
        }
    }
    return cmd;
}

toolchain toolchain::get_builtin(const std::string_view tc_id_) {
    BPT_E_SCOPE(bpt::e_builtin_toolchain_str{std::string(tc_id_)});
    auto tc_id = tc_id_;
    using namespace std::literals;

    json5::data tc_data  = json5::data::mapping_type();
    auto&       root_map = tc_data.as_object();

    auto handle_prefix = [&](std::string_view key, std::string_view prefix) {
        assert(!prefix.empty());
        assert(prefix.back() == ':');
        if (starts_with(tc_id, prefix)) {
            tc_id.remove_prefix(prefix.length());
            prefix.remove_suffix(1);  // remove trailing :
            root_map.emplace(key, std::string(prefix));
            return true;
        }
        return false;
    };

    if (starts_with(tc_id, "debug:")) {
        tc_id = tc_id.substr("debug:"sv.length());
        root_map.emplace("debug", true);
    }

    handle_prefix("compiler_launcher", "ccache:");

    for (std::string_view prefix : {"c++98:", "c++03:", "c++11:", "c++14:", "c++17:", "c++20:"}) {
        if (handle_prefix("cxx_version", prefix)) {
            break;
        }
    }

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
        BOOST_LEAF_THROW_EXCEPTION(e_human_message{neo::ufmt("Invalid toolchain string '{}'",
                                                             tc_id)},
                                   BPT_ERR_REF("invalid-builtin-toolchain"));
    }

    root_map.emplace("c_compiler", opt_triple->c);
    root_map.emplace("cxx_compiler", opt_triple->cxx);
    root_map.emplace("compiler_id", opt_triple->id);
    return parse_toolchain_json_data(tc_data);
}

bpt::toolchain bpt::toolchain::get_default() {
    using namespace std::literals;
    auto dirs       = {fs::current_path(), bpt_config_dir(), user_home_dir()};
    auto extensions = {".yaml", ".jsonc", ".json5", ".json"};
    auto candidates = {
        fs::current_path() / "toolchain.json5",
        fs::current_path() / "toolchain.jsonc",
        fs::current_path() / "toolchain.json",
        bpt_config_dir() / "toolchain.json5",
        bpt_config_dir() / "toolchain.jsonc",
        bpt_config_dir() / "toolchain.json",
        user_home_dir() / "toolchain.json5",
        user_home_dir() / "toolchain.jsonc",
        user_home_dir() / "toolchain.json",
    };
    for (auto&& [ext, dir] : ranges::view::cartesian_product(extensions, dirs)) {
        fs::path cand = dir / ("toolchain"s + ext);
        bpt_log(trace, "Checking for default toolchain at [{}]", cand.string());
        if (not fs::is_regular_file(cand)) {
            continue;
        }
        bpt_log(debug, "Using default toolchain file: {}", cand.string());
        return toolchain::from_file(cand);
    }
    BOOST_LEAF_THROW_EXCEPTION(e_human_message{neo::ufmt("No default toolchain")},
                               BPT_ERR_REF("no-default-toolchain"),
                               e_error_marker{"no-default-toolchain"});
}

toolchain toolchain::from_file(path_ref fpath) {
    if (fpath.extension().string() == ".yaml") {
        auto node = bpt::parse_yaml_file(fpath);
        auto data = bpt::yaml_as_json5_data(node);
        return parse_toolchain_json_data(data,
                                         neo::ufmt("Loading toolchain from [{}]", fpath.string()));
    } else {
        return parse_toolchain_json5(bpt::read_file(fpath));
    }
}