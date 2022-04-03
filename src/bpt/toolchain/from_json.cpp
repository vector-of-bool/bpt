#include "./from_json.hpp"

#include <bpt/dym.hpp>
#include <bpt/error/errors.hpp>
#include <bpt/toolchain/prep.hpp>
#include <bpt/util/algo.hpp>
#include <bpt/util/shlex.hpp>
#include <bpt/util/string.hpp>

#include <fmt/core.h>
#include <json5/parse_data.hpp>
#include <semester/walk.hpp>

#include <string>

using namespace bpt;

using std::optional;
using std::string;
using std::vector;
using string_seq     = vector<string>;
using opt_string     = optional<string>;
using opt_string_seq = optional<string_seq>;
using strv           = std::string_view;

namespace {

template <typename T, typename Func>
T read_opt(const std::optional<T>& what, Func&& fn) {
    if (!what.has_value()) {
        return fn();
    }
    return *what;
}

template <typename... Args>
[[noreturn]] void fail(strv context, strv message, Args&&... args) {
    auto fmtd = fmt::format(message, args...);
    throw std::runtime_error(fmt::format("{} - Failed to read toolchain file: {}", context, fmtd));
}

}  // namespace

toolchain bpt::parse_toolchain_json5(std::string_view j5_str, std::string_view context) {
    auto dat = json5::parse_data(j5_str);
    return parse_toolchain_json_data(dat, context);
}

toolchain bpt::parse_toolchain_json_data(const json5::data& dat, std::string_view context) {
    using namespace semester;

    opt_string     compiler_id;
    opt_string     c_compiler;
    opt_string     cxx_compiler;
    opt_string     c_version;
    opt_string     cxx_version;
    opt_string_seq compiler_launcher;

    opt_string_seq common_flags;
    opt_string_seq c_flags;
    opt_string_seq cxx_flags;
    opt_string_seq link_flags;
    opt_string_seq warning_flags;

    optional<bool> debug_bool;
    opt_string     debug_str;
    optional<bool> do_optimize;
    optional<bool> runtime_static;
    optional<bool> runtime_debug;

    // Advanced-mode:
    opt_string     deps_mode_str;
    opt_string     archive_prefix;
    opt_string     archive_suffix;
    opt_string     obj_prefix;
    opt_string     obj_suffix;
    opt_string     exe_prefix;
    opt_string     exe_suffix;
    opt_string_seq base_warning_flags;
    opt_string_seq base_flags;
    opt_string_seq base_c_flags;
    opt_string_seq base_cxx_flags;
    opt_string_seq include_template;
    opt_string_seq external_include_template;
    opt_string_seq define_template;
    opt_string_seq c_compile_file;
    opt_string_seq cxx_compile_file;
    opt_string_seq create_archive;
    opt_string_seq link_executable;
    opt_string_seq tty_flags;
    opt_string     lang_version_flag_template;

    opt_string_seq c_source_type_flags;
    opt_string_seq cxx_source_type_flags;
    opt_string_seq syntax_only_flags;
    opt_string_seq consider_env;
    // For copy-pasting convenience: ‘{}’

    auto extend_flags = [&](string key, auto& opt_flags) {
        return [&opt_flags, key](const json5::data& dat) {
            if (!opt_flags) {
                opt_flags.emplace();
            }
            walk(  //
                dat,
                if_type<string>([&](auto& str_) {
                    auto more_flags = split_shell_string(str_.as_string());
                    extend(*opt_flags, more_flags);
                    return walk.accept;
                }),
                if_array{for_each{
                    require_type<string>{
                        fmt::format("Elements of `{}` array must be strings", key)},
                    put_into{std::back_inserter(*opt_flags)},
                }},
                reject_with(fmt::format("`{}` must be an array or a shell-like string", key)));
            return walk.accept;
        };
    };

#define KEY_EXTEND_FLAGS(Name)                                                                     \
    if_key { #Name, extend_flags(#Name, Name) }

#define KEY_STRING(Name)                                                                           \
    if_key{                                                                                        \
        #Name,                                                                                     \
        require_type<string>("`" #Name "` must be a string"),                                      \
        put_into{Name},                                                                            \
    }

    walk(  //
        dat,
        require_type<json5::data::mapping_type>("Root of toolchain data must be a mapping"),
        mapping{
            if_key{"$schema", just_accept},
            KEY_STRING(compiler_id),
            KEY_STRING(c_compiler),
            KEY_STRING(cxx_compiler),
            KEY_STRING(c_version),
            KEY_STRING(cxx_version),
            KEY_EXTEND_FLAGS(c_flags),
            KEY_EXTEND_FLAGS(cxx_flags),
            KEY_EXTEND_FLAGS(warning_flags),
            KEY_EXTEND_FLAGS(link_flags),
            KEY_EXTEND_FLAGS(compiler_launcher),
            if_key{"debug",
                   if_type<bool>(put_into{debug_bool}),
                   if_type<std::string>(put_into{debug_str}),
                   reject_with("'debug' must be a bool or string")},
            if_key{"optimize",
                   require_type<bool>("`optimize` must be a boolean value"),
                   put_into{do_optimize}},
            if_key{"flags", extend_flags("flags", common_flags)},
            if_key{"runtime",
                   require_type<json5::data::mapping_type>("'runtime' must be a JSON object"),
                   mapping{if_key{"static",
                                  require_type<bool>("'/runtime/static' should be a boolean"),
                                  put_into(runtime_static)},
                           if_key{"debug",
                                  require_type<bool>("'/runtime/debug' should be a boolean"),
                                  put_into(runtime_debug)}}},
            if_key{
                "advanced",
                require_type<json5::data::mapping_type>("`advanced` must be a mapping"),
                mapping{
                    if_key{"deps_mode",
                           require_type<string>("`deps_mode` must be a string"),
                           put_into{deps_mode_str}},
                    KEY_EXTEND_FLAGS(include_template),
                    KEY_EXTEND_FLAGS(external_include_template),
                    KEY_EXTEND_FLAGS(define_template),
                    KEY_EXTEND_FLAGS(base_warning_flags),
                    KEY_EXTEND_FLAGS(base_flags),
                    KEY_EXTEND_FLAGS(base_c_flags),
                    KEY_EXTEND_FLAGS(base_cxx_flags),
                    KEY_EXTEND_FLAGS(c_compile_file),
                    KEY_EXTEND_FLAGS(cxx_compile_file),
                    KEY_EXTEND_FLAGS(create_archive),
                    KEY_EXTEND_FLAGS(link_executable),
                    KEY_EXTEND_FLAGS(tty_flags),
                    KEY_STRING(obj_prefix),
                    KEY_STRING(obj_suffix),
                    KEY_STRING(archive_prefix),
                    KEY_STRING(archive_suffix),
                    KEY_STRING(exe_prefix),
                    KEY_STRING(exe_suffix),
                    KEY_STRING(lang_version_flag_template),
                    KEY_EXTEND_FLAGS(c_source_type_flags),
                    KEY_EXTEND_FLAGS(cxx_source_type_flags),
                    KEY_EXTEND_FLAGS(syntax_only_flags),
                    KEY_EXTEND_FLAGS(consider_env),
                    [&](auto key, auto) -> walk_result {
                        auto dym = did_you_mean(key,
                                                {
                                                    "deps_mode",
                                                    "include_template",
                                                    "external_include_template",
                                                    "define_template",
                                                    "base_warning_flags",
                                                    "base_flags",
                                                    "base_c_flags",
                                                    "base_cxx_flags",
                                                    "c_compile_file",
                                                    "cxx_compile_file",
                                                    "create_archive",
                                                    "link_executable",
                                                    "obj_prefix",
                                                    "obj_suffix",
                                                    "archive_prefix",
                                                    "archive_suffix",
                                                    "exe_prefix",
                                                    "exe_suffix",
                                                    "tty_flags",
                                                    "lang_version_flag_template",
                                                    "c_source_type_flags",
                                                    "cxx_source_type_flags",
                                                    "syntax_only_flags",
                                                    "consider_env",
                                                });
                        fail(context,
                             "Unknown toolchain advanced-config key ‘{}’ (Did you mean ‘{}’?)",
                             key,
                             *dym);
                    },
                },
            },
            [&](auto key, auto &&) -> walk_result {
                // They've given an unknown key. Ouch.
                auto dym = did_you_mean(key,
                                        {
                                            "compiler_id",
                                            "c_compiler",
                                            "cxx_compiler",
                                            "c_version",
                                            "cxx_version",
                                            "c_flags",
                                            "cxx_flags",
                                            "warning_flags",
                                            "link_flags",
                                            "flags",
                                            "debug",
                                            "optimize",
                                            "runtime",
                                        });
                fail(context, "Unknown toolchain config key ‘{}’ (Did you mean ‘{}’?)", key, *dym);
            },
        });

    if (debug_str.has_value() && debug_str != "embedded" && debug_str != "split"
        && debug_str != "none") {
        fail(context, "'debug' string must be one of 'none', 'embedded', or 'split'");
    }

    enum compiler_id_e_t {
        no_comp_id,
        msvc,
        clang,
        gnu,
    } compiler_id_e
        = [&] {
              if (!compiler_id) {
                  return no_comp_id;
              } else if (compiler_id == "msvc") {
                  return msvc;
              } else if (compiler_id == "gnu") {
                  return gnu;
              } else if (compiler_id == "clang") {
                  return clang;
              } else {
                  fail(context, "Invalid `compiler_id` value ‘{}’", *compiler_id);
              }
          }();

    bool is_gnu      = compiler_id_e == gnu;
    bool is_clang    = compiler_id_e == clang;
    bool is_msvc     = compiler_id_e == msvc;
    bool is_gnu_like = is_gnu || is_clang;

    if (!lang_version_flag_template.has_value()) {
        if (is_gnu_like) {
            lang_version_flag_template = "-std=[version]";
        } else if (is_msvc) {
            lang_version_flag_template = "/std:[version]";
        }
    }

    const enum file_deps_mode deps_mode = [&] {
        if (!deps_mode_str.has_value()) {
            if (is_gnu_like) {
                return file_deps_mode::gnu;
            } else if (is_msvc) {
                return file_deps_mode::msvc;
            } else {
                return file_deps_mode::none;
            }
        } else if (deps_mode_str == "gnu") {
            return file_deps_mode::gnu;
        } else if (deps_mode_str == "msvc") {
            return file_deps_mode::msvc;
        } else if (deps_mode_str == "none") {
            return file_deps_mode::none;
        } else {
            fail(context, "Invalid `deps_mode` value ‘{}’", *deps_mode_str);
        }
    }();

    // Now convert the flags we've been given into a real toolchain
    auto get_compiler_executable_path = [&](language lang) -> string {
        if (lang == language::cxx && cxx_compiler) {
            return *cxx_compiler;
        }
        if (lang == language::c && c_compiler) {
            return *c_compiler;
        }
        if (!compiler_id.has_value()) {
            if (lang == language::c) {
                fail(context, "Unable to determine the executable for a C compiler");
            }
            if (lang == language::cxx) {
                fail(context, "Unable to determine the executable for a C++ compiler");
            }
        }
        if (is_gnu) {
            return (lang == language::cxx) ? "g++" : "gcc";
        }
        if (is_clang) {
            return (lang == language::cxx) ? "clang++" : "clang";
        }
        if (is_msvc) {
            return "cl.exe";
        }
        assert(false && "Compiler name deduction failed");
        std::terminate();
    };

    auto get_c_version_flags = [&]() -> string_seq {
        if (!c_version) {
            return {};
        }
        if (!lang_version_flag_template) {
            if (!compiler_id) {
                fail(context,
                     "Unable to deduce flags for 'c_version' without setting 'compiler_id' or "
                     "'lang_version_flag_template'");
            } else {
                fail(context,
                     "Unable to determine the 'lang_version_flag_template' for the given "
                     "'compiler_id', required to generate the language version flags for "
                     "'c_version'");
            }
        }
        auto flag = replace(*lang_version_flag_template, "[version]", *c_version);
        return {flag};
    };

    auto get_cxx_version_flags = [&]() -> string_seq {
        if (!cxx_version) {
            return {};
        }
        if (!lang_version_flag_template) {
            if (!compiler_id) {
                fail(context,
                     "Unable to deduce flags for 'cxx_version' without setting 'compiler_id' or "
                     "'lang_version_flag_template'");
            } else {
                fail(context,
                     "Unable to determine the 'lang_version_flag_template' for the given "
                     "'compiler_id', required to generate the language version flags for "
                     "'cxx_version'");
            }
        }
        auto flag = replace(*lang_version_flag_template, "[version]", *cxx_version);
        return {flag};
    };

    auto get_runtime_flags = [&]() -> string_seq {
        string_seq ret;
        if (is_msvc) {
            std::string rt_flag = "/M";
            // Select debug/release runtime flag. Default is 'true'
            if (runtime_static.value_or(true)) {
                rt_flag.push_back('T');
            } else {
                rt_flag.push_back('D');
            }
            if (runtime_debug.value_or(debug_bool.value_or(false) || debug_str == "embedded"
                                       || debug_str == "split")) {
                rt_flag.push_back('d');
            }
            ret.push_back(rt_flag);
        } else if (is_gnu_like) {
            if (runtime_static.value_or(false)) {
                extend(ret, {"-static-libgcc", "-static-libstdc++"});
            }
            if (runtime_debug.value_or(false)) {
                extend(ret, {"-D_GLIBCXX_DEBUG", "-D_LIBCPP_DEBUG=1"});
            }
        } else {
            // No flags if we don't know the compiler
        }

        return ret;
    };

    auto get_optim_flags = [&]() -> string_seq {
        if (do_optimize != true) {
            return {};
        }
        if (is_msvc) {
            return {"/O2"};
        } else if (is_gnu_like) {
            return {"-O2"};
        } else {
            return {};
        }
    };

    auto get_debug_flags = [&]() -> string_seq {
        if (is_msvc) {
            if (debug_bool == true || debug_str == "embedded") {
                return {"/Z7"};
            } else if (debug_str == "split") {
                return {"/Zi", "/FS"};
            } else {
                // Do not generate any debug infro
                return {};
            }
        } else if (is_gnu_like) {
            if (debug_bool == true || debug_str == "embedded") {
                return {"-g"};
            } else if (debug_str == "split") {
                return {"-g", "-gsplit-dwarf"};
            } else {
                // Do not generate debug info
                return {};
            }
        } else {
            // Cannont deduce the debug flags
            return {};
        }
    };

    auto get_link_flags = [&]() -> string_seq {
        string_seq ret;
        extend(ret, get_runtime_flags());
        extend(ret, get_optim_flags());
        extend(ret, get_debug_flags());
        if (link_flags) {
            extend(ret, *link_flags);
        }
        return ret;
    };

    auto get_default_compile_command = [&]() -> string_seq {
        string_seq ret;
        if (is_msvc) {
            extend(ret, {"[flags]", "/c", "[in]", "/Fo[out]"});
        } else if (is_gnu_like) {
            extend(ret, {"[flags]", "-c", "[in]", "-o[out]"});
        }
        return ret;
    };

    auto get_base_flags = [&](language lang) -> string_seq {
        string_seq ret;
        if (base_flags) {
            extend(ret, *base_flags);
        } else if (is_msvc) {
            extend(ret, {"/nologo", "/permissive-"});
        } else if (is_gnu_like) {
            extend(ret, {"-fPIC", "-pthread"});
        }
        if (lang == language::c && base_c_flags) {
            extend(ret, *base_c_flags);
        }
        if (lang == language::cxx) {
            if (base_cxx_flags) {
                extend(ret, *base_cxx_flags);
            } else if (is_msvc) {
                extend(ret, {"/EHsc"});
            }
        }
        return ret;
    };

    auto get_flags = [&](language lang) -> string_seq {
        string_seq ret;
        extend(ret, get_runtime_flags());
        extend(ret, get_optim_flags());
        extend(ret, get_debug_flags());
        if (common_flags) {
            extend(ret, *common_flags);
        }
        if (lang == language::cxx) {
            if (cxx_flags) {
                extend(ret, *cxx_flags);
            }
            extend(ret, get_cxx_version_flags());
        }
        if (lang == language::c) {
            if (c_flags) {
                extend(ret, *c_flags);
            }
            extend(ret, get_c_version_flags());
        }
        extend(ret, get_base_flags(lang));
        return ret;
    };

    toolchain_prep tc;
    tc.deps_mode = deps_mode;
    tc.c_compile = read_opt(c_compile_file, [&] {
        string_seq c;
        if (compiler_launcher) {
            extend(c, *compiler_launcher);
        }
        c.push_back(get_compiler_executable_path(language::c));
        extend(c, get_default_compile_command());
        return c;
    });
    extend(tc.c_compile, get_flags(language::c));

    tc.cxx_compile = read_opt(cxx_compile_file, [&] {
        string_seq cxx;
        if (compiler_launcher) {
            extend(cxx, *compiler_launcher);
        }
        cxx.push_back(get_compiler_executable_path(language::cxx));
        extend(cxx, get_default_compile_command());
        return cxx;
    });
    extend(tc.cxx_compile, get_flags(language::cxx));

    tc.consider_envs = read_opt(consider_env, [&]() -> string_seq {
        if (is_msvc) {
            return {"CL", "_CL_", "INCLUDE", "LIBPATH", "LIB"};
        }
        return {};
    });

    tc.include_template = read_opt(include_template, [&]() -> string_seq {
        if (!compiler_id) {
            fail(context, "Cannot deduce 'include_template' without 'compiler_id'");
        }
        if (is_gnu_like) {
            return {"-I", "[path]"};
        } else if (is_msvc) {
            return {"/I", "[path]"};
        }
        assert(false && "'include_template' deduction failed");
        std::terminate();
    });

    tc.external_include_template = read_opt(external_include_template, [&]() -> string_seq {
        if (!compiler_id) {
            // Just reuse the include template for regular files
            return tc.include_template;
        }
        if (is_gnu_like) {
            return {"-isystem", "[path]"};
        } else if (is_msvc) {
            // MSVC has external-header support inbound, but it is not fully ready yet
            return {"/I", "[path]"};
        }
        assert(false && "external_include_template deduction failed");
        std::terminate();
    });

    tc.define_template = read_opt(define_template, [&]() -> string_seq {
        if (!compiler_id) {
            fail(context, "Cannot deduce 'define_template' without 'compiler_id'");
        }
        if (is_gnu_like) {
            return {"-D", "[def]"};
        } else if (is_msvc) {
            return {"/D", "[def]"};
        }
        assert(false && "define_template deduction failed");
        std::terminate();
    });

    tc.archive_prefix = archive_prefix.value_or("lib");
    tc.archive_suffix = read_opt(archive_suffix, [&] {
        if (!compiler_id) {
            fail(context, "Cannot deduce library file extension without 'compiler_id'");
        }
        if (is_gnu_like) {
            return ".a";
        } else if (is_msvc) {
            return ".lib";
        }
        assert(false && "No archive suffix");
        std::terminate();
    });

    tc.object_prefix = obj_prefix.value_or("");
    tc.object_suffix = read_opt(obj_suffix, [&] {
        if (!compiler_id) {
            fail(context, "Cannot deduce object file extension without 'compiler_id'");
        }
        if (is_gnu_like) {
            return ".o";
        } else if (is_msvc) {
            return ".obj";
        }
        assert(false && "No object file suffix");
        std::terminate();
    });

    tc.exe_prefix = exe_prefix.value_or("");
    tc.exe_suffix = read_opt(exe_suffix, [&] {
#ifdef _WIN32
        return ".exe";
#else
        return "";
#endif
    });

    // `base_warning_flags` is loaded first, and switches based on compiler_id.
    // This one is "advanced," and sets a common base for warning sets
    tc.warning_flags = read_opt(base_warning_flags, [&]() -> string_seq {
        if (!compiler_id) {
            // No error. Just no warning flags
            return {};
        }
        if (is_msvc) {
            return {"/W4"};
        } else if (is_gnu_like) {
            return {"-Wall", "-Wextra", "-Wpedantic", "-Wconversion"};
        }
        assert(false && "No warning flags");
        std::terminate();
    });

    // `warning_flags` allows the user to provide additional warnings/errors
    if (warning_flags) {
        extend(tc.warning_flags, *warning_flags);
    }

    tc.link_archive = read_opt(create_archive, [&]() -> string_seq {
        if (!compiler_id) {
            fail(context, "Unable to deduce archive creation rules without a 'compiler_id'");
        }
        if (is_msvc) {
            return {"lib", "/nologo", "/OUT:[out]", "[in]"};
        } else if (is_gnu_like) {
            return {"ar", "rcs", "[out]", "[in]"};
        }
        assert(false && "No archive command");
        std::terminate();
    });

    tc.link_exe = read_opt(link_executable, [&]() -> string_seq {
        if (!compiler_id) {
            fail(context, "Unable to deduce how to link executables without a 'compiler_id'");
        }
        string_seq ret;
        if (is_msvc) {
            ret = {get_compiler_executable_path(language::cxx),
                   "/nologo",
                   "/EHsc",
                   "[in]",
                   "/Fe[out]"};
        } else if (is_gnu_like) {
            ret = {get_compiler_executable_path(language::cxx),
                   "-fPIC",
                   "[in]",
                   "-pthread",
                   "-o[out]"};
        } else {
            assert(false && "No link-exe command");
            std::terminate();
        }
        return ret;
    });
    extend(tc.link_exe, get_link_flags());

    tc.tty_flags = read_opt(tty_flags, [&]() -> string_seq {
        if (!compiler_id) {
            // Don't deduce any flags. This is a non-error, as these flags should be purely
            // aesthetic
            return {};
        }
        if (is_msvc) {
            // MSVC doesn't have any special TTY flags (yet...)
            return {};
        } else if (is_gnu_like) {
            return {"-fdiagnostics-color"};
        } else {
            assert(false && "Impossible compiler_id while deducing `tty_flags`");
            std::terminate();
        }
    });

    tc.c_source_type_flags = read_opt(c_source_type_flags, [&]() -> string_seq {
        if (!compiler_id) {
            fail(context, "Unable to deduce C source type flags without a 'compiler_id'");
        }
        if (is_msvc) {
            return {"/TC"};
        } else if (is_gnu_like) {
            return {"-xc"};
        } else {
            assert(false && "Impossible compiler_id while deducing `c_source_type_flags`");
            std::terminate();
        }
    });

    tc.cxx_source_type_flags = read_opt(cxx_source_type_flags, [&]() -> string_seq {
        if (!compiler_id) {
            fail(context, "Unable to deduce C++ source type flags without a 'compiler_id'");
        }
        if (is_msvc) {
            return {"/TP"};
        } else if (is_gnu_like) {
            return {"-xc++"};
        } else {
            assert(false && "Impossible compiler_id while deducing `cxx_source_type_flags`");
            std::terminate();
        }
    });

    tc.syntax_only_flags = read_opt(syntax_only_flags, [&]() -> string_seq {
        if (!compiler_id) {
            fail(context, "Unable to deduce C++ syntax only flags without a 'compiler_id'");
        }
        if (is_msvc) {
            return {"/Zs"};
        } else if (is_gnu_like) {
            return {"-fsyntax-only"};
        } else {
            assert(false && "Impossible compiler_id while deducing `syntax_only_flags`");
            std::terminate();
        }
    });

    return tc.realize();
}
