#include "./from_dds.hpp"

#include <dds/toolchain/prep.hpp>
#include <dds/toolchain/toolchain.hpp>
#include <dds/util/algo.hpp>
#include <libman/parse.hpp>

#include <spdlog/fmt/fmt.h>

#include <map>
#include <optional>
#include <tuple>
#include <vector>

using namespace dds;

using fmt::format;
using std::optional;
using std::string;
using std::vector;
using string_seq  = vector<string>;
using opt_string  = optional<string>;
using opt_str_seq = optional<string_seq>;
using strv        = std::string_view;

toolchain dds::parse_toolchain_dds(strv str, strv context) {
    auto kvs = lm::parse_string(str);
    return parse_toolchain_dds(kvs, context);
}

namespace {

struct read_argv_acc {
    strv         my_key;
    opt_str_seq& out;

    bool operator()(strv, strv key, strv value) const {
        if (key != my_key) {
            return false;
        }
        if (!out) {
            out.emplace();
        }
        auto cmd = split_shell_string(value);
        extend(*out, cmd);
        return true;
    }
};

struct read_argv {
    strv         my_key;
    opt_str_seq& out;

    bool operator()(strv ctx, strv key, strv value) const {
        if (key != my_key) {
            return false;
        }
        if (out.has_value()) {
            throw std::runtime_error(
                format("{}: More than one value provided for key '{}'", ctx, key));
        }
        out.emplace(split_shell_string(value));
        return true;
    }
};

template <typename T, typename Func>
T read_opt(const std::optional<T>& what, Func&& fn) {
    if (!what.has_value()) {
        return fn();
    }
    return *what;
}

template <typename... Args>
[[noreturn]] void fail(strv context, strv message, Args&&... args) {
    throw std::runtime_error(
        format("{} - Failed to read toolchain file: {}", context, message, args...));
}
}  // namespace

toolchain dds::parse_toolchain_dds(const lm::pair_list& pairs, strv context) {
    opt_string     compiler_id;
    opt_string     c_compiler_fpath;
    opt_string     cxx_compiler_fpath;
    opt_string     c_version;
    opt_string     cxx_version;
    opt_string     archive_prefix;
    opt_string     archive_suffix;
    opt_string     obj_prefix;
    opt_string     obj_suffix;
    opt_string     exe_prefix;
    opt_string     exe_suffix;
    optional<bool> do_debug;
    optional<bool> do_optimize;
    opt_str_seq    include_template;
    opt_str_seq    define_template;
    opt_str_seq    warning_flags;
    opt_str_seq    flags;
    opt_str_seq    c_flags;
    opt_str_seq    cxx_flags;
    opt_str_seq    c_compile_file;
    opt_str_seq    cxx_compile_file;
    opt_str_seq    create_archive;
    opt_str_seq    link_executable;
    opt_str_seq    compile_launcher;

    lm::read(context,
             pairs,
             // Base compile info:
             lm::read_opt("Compiler-ID", compiler_id),
             lm::read_opt("C-Compiler", c_compiler_fpath),
             lm::read_opt("C++-Compiler", cxx_compiler_fpath),
             // Language options
             lm::read_opt("C-Version", c_version),
             lm::read_opt("C++-Version", cxx_version),
             // Flag templates
             read_argv{"Include-Template", include_template},
             read_argv{"Define-Template", define_template},
             // Flags
             read_argv_acc{"Warning-Flags", warning_flags},
             read_argv_acc{"Flags", flags},
             read_argv_acc{"C-Flags", c_flags},
             read_argv_acc{"C++-Flags", cxx_flags},
             // Options for flags
             lm::read_bool("Optimize", do_optimize),
             lm::read_bool("Debug", do_debug),
             // Miscellaneous
             read_argv{"Compiler-Launcher", compile_launcher},
             // Command templates
             read_argv{"C-Compile-File", c_compile_file},
             read_argv{"C++-Compile-File", cxx_compile_file},
             read_argv{"Create-Archive", create_archive},
             read_argv{"Link-Executable", link_executable},
             // Filename affixes
             lm::read_opt("Archive-Prefix", archive_prefix),
             lm::read_opt("Archive-Suffix", archive_suffix),
             lm::read_opt("Object-Prefix", obj_prefix),
             lm::read_opt("Object-Suffix", obj_suffix),
             lm::read_opt("Executable-Prefix", exe_prefix),
             lm::read_opt("Executable-Suffix", exe_suffix),
             // Die:
             lm::reject_unknown());

    toolchain_prep tc;

    enum compiler_id_e_t {
        no_comp_id,
        msvc,
        clang,
        gnu,
    } compiler_id_e
        = [&] {
              if (!compiler_id) {
                  return no_comp_id;
              } else if (compiler_id == "MSVC") {
                  return msvc;
              } else if (compiler_id == "GNU") {
                  return gnu;
              } else if (compiler_id == "Clang") {
                  return clang;
              } else {
                  fail(context, "Unknown Compiler-ID '{}'", *compiler_id);
              }
          }();

    bool is_gnu      = compiler_id_e == gnu;
    bool is_clang    = compiler_id_e == clang;
    bool is_msvc     = compiler_id_e == msvc;
    bool is_gnu_like = is_gnu || is_clang;

    // Now convert the flags we've been given into a real toolchain
    auto get_compiler = [&](bool is_cxx) -> string {
        if (is_cxx && cxx_compiler_fpath) {
            return *cxx_compiler_fpath;
        }
        if (!is_cxx && c_compiler_fpath) {
            return *c_compiler_fpath;
        }
        if (!compiler_id.has_value()) {
            fail(context, "Unable to determine what compiler to use.");
        }
        if (is_gnu) {
            return is_cxx ? "g++" : "gcc";
        }
        if (is_clang) {
            return is_cxx ? "clang++" : "clang";
        }
        if (is_msvc) {
            return "cl.exe";
        }
        assert(false && "Compiler name deduction failed");
        std::terminate();
    };

    enum c_version_e_t {
        c_none,
        c89,
        c99,
        c11,
        c18,
    } c_version_e
        = [&] {
              if (!c_version) {
                  return c_none;
              } else if (c_version == "c89") {
                  return c89;
              } else if (c_version == "c99") {
                  return c99;
              } else if (c_version == "c11") {
                  return c11;
              } else if (c_version == "c18") {
                  return c18;
              } else {
                  fail(context, "Unknown C-Version '{}'", *c_version);
              }
          }();

    enum cxx_version_e_t {
        cxx_none,
        cxx98,
        cxx03,
        cxx11,
        cxx14,
        cxx17,
        cxx20,
    } cxx_version_e
        = [&] {
              if (!cxx_version) {
                  return cxx_none;
              } else if (cxx_version == "c++98") {
                  return cxx98;
              } else if (cxx_version == "c++03") {
                  return cxx03;
              } else if (cxx_version == "c++11") {
                  return cxx11;
              } else if (cxx_version == "c++14") {
                  return cxx14;
              } else if (cxx_version == "c++17") {
                  return cxx17;
              } else if (cxx_version == "c++20") {
                  return cxx20;
              } else {
                  fail(context, "Unknown C++-Version '{}'", *cxx_version);
              }
          }();

    std::map<std::tuple<compiler_id_e_t, c_version_e_t>, string_seq> c_version_flag_table = {
        {{msvc, c_none}, {}},
        {{msvc, c89}, {}},
        {{msvc, c99}, {}},
        {{msvc, c11}, {}},
        {{msvc, c18}, {}},
        {{gnu, c_none}, {}},
        {{gnu, c89}, {"-std=c89"}},
        {{gnu, c99}, {"-std=c99"}},
        {{gnu, c11}, {"-std=c11"}},
        {{gnu, c18}, {"-std=c18"}},
        {{clang, c_none}, {}},
        {{clang, c89}, {"-std=c89"}},
        {{clang, c99}, {"-std=c99"}},
        {{clang, c11}, {"-std=c11"}},
        {{clang, c18}, {"-std=c18"}},
    };

    auto get_c_version_flags = [&]() -> string_seq {
        if (!compiler_id.has_value()) {
            fail(context, "Unable to deduce flags for 'C-Version' without setting 'Compiler-ID'");
        }
        auto c_ver_iter = c_version_flag_table.find({compiler_id_e, c_version_e});
        assert(c_ver_iter != c_version_flag_table.end());
        return c_ver_iter->second;
    };

    std::map<std::tuple<compiler_id_e_t, cxx_version_e_t>, string_seq> cxx_version_flag_table = {
        {{msvc, cxx_none}, {}},
        {{msvc, cxx98}, {}},
        {{msvc, cxx03}, {}},
        {{msvc, cxx11}, {}},
        {{msvc, cxx14}, {"/std:c++14"}},
        {{msvc, cxx17}, {"/std:c++17"}},
        {{msvc, cxx20}, {"/std:c++latest"}},
        {{gnu, cxx_none}, {}},
        {{gnu, cxx98}, {"-std=c++98"}},
        {{gnu, cxx03}, {"-std=c++03"}},
        {{gnu, cxx11}, {"-std=c++11"}},
        {{gnu, cxx14}, {"-std=c++14"}},
        {{gnu, cxx17}, {"-std=c++17"}},
        {{gnu, cxx20}, {"-std=c++20"}},
        {{clang, cxx_none}, {}},
        {{clang, cxx98}, {"-std=c++98"}},
        {{clang, cxx03}, {"-std=c++03"}},
        {{clang, cxx11}, {"-std=c++11"}},
        {{clang, cxx14}, {"-std=c++14"}},
        {{clang, cxx17}, {"-std=c++17"}},
        {{clang, cxx20}, {"-std=c++20"}},
    };

    auto get_cxx_version_flags = [&]() -> string_seq {
        if (!compiler_id.has_value()) {
            fail(context, "Unable to deduce flags for 'C++-Version' without setting 'Compiler-ID'");
        }
        auto cxx_ver_iter = cxx_version_flag_table.find({compiler_id_e, cxx_version_e});
        assert(cxx_ver_iter != cxx_version_flag_table.end());
        return cxx_ver_iter->second;
    };

    auto get_flags = [&](bool is_cxx) -> string_seq {
        string_seq ret;
        if (flags) {
            extend(ret, *flags);
        }
        if (is_cxx && cxx_flags) {
            extend(ret, *cxx_flags);
        }
        if (!is_cxx && c_flags) {
            extend(ret, *c_flags);
        }
        if (!is_cxx && c_version) {
            extend(ret, get_c_version_flags());
        }
        if (is_cxx && cxx_version) {
            extend(ret, get_cxx_version_flags());
        }
        if (is_msvc) {
            strv rt_lib = "/MT";
            if (do_optimize.has_value() && *do_optimize) {
                extend(ret, {"/O2"});
            }
            if (do_debug.has_value() && *do_debug) {
                extend(ret, {"/Z7", "/DEBUG", "/MTd"});
                rt_lib = "/MTd";
            }
            ret.emplace_back(rt_lib);
            extend(ret, {"/nologo", "<FLAGS>", "/c", "<IN>", "/permissive-", "/Fo<OUT>"});
            if (is_cxx) {
                extend(ret, {"/EHsc"});
            }
        } else if (is_gnu_like) {
            if (do_optimize.has_value() && *do_optimize) {
                extend(ret, {"-O2"});
            }
            if (do_debug.has_value() && *do_debug) {
                extend(ret, {"-g"});
            }
            extend(ret,
                   {"-fPIC",
                    "-fdiagnostics-color",
                    "-pthread",
                    "<FLAGS>",
                    "-c",
                    "<IN>",
                    "-o<OUT>"});
        }
        return ret;
    };

    tc.c_compile = read_opt(c_compile_file, [&] {
        string_seq c;
        if (compile_launcher) {
            extend(c, *compile_launcher);
        }
        c.push_back(get_compiler(false));
        extend(c, get_flags(false));
        return c;
    });

    tc.cxx_compile = read_opt(cxx_compile_file, [&] {
        string_seq cxx;
        if (compile_launcher) {
            extend(cxx, *compile_launcher);
        }
        cxx.push_back(get_compiler(true));
        extend(cxx, get_flags(true));
        return cxx;
    });

    tc.include_template = read_opt(include_template, [&]() -> string_seq {
        if (!compiler_id) {
            fail(context, "Cannot deduce 'Include-Template' without 'Compiler-ID'");
        }
        if (is_gnu_like) {
            return {"-I", "<PATH>"};
        } else if (compiler_id == "MSVC") {
            return {"/I", "<PATH>"};
        }
        assert(false && "Include-Template deduction failed");
        std::terminate();
    });

    tc.define_template = read_opt(define_template, [&]() -> string_seq {
        if (!compiler_id) {
            fail(context, "Cannot deduce 'Define-Template' without 'Compiler-ID'");
        }
        if (is_gnu_like) {
            return {"-D", "<DEF>"};
        } else if (compiler_id == "MSVC") {
            return {"/D", "<DEF>"};
        }
        assert(false && "Define-Template deduction failed");
        std::terminate();
    });

    tc.archive_prefix = archive_prefix.value_or("lib");
    tc.archive_suffix = read_opt(archive_suffix, [&] {
        if (!compiler_id) {
            fail(context, "Cannot deduce library file extension without Compiler-ID");
        }
        if (is_gnu) {
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
            fail(context, "Cannot deduce object file extension without Compiler-ID");
        }
        if (is_gnu) {
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

    tc.warning_flags = read_opt(warning_flags, [&]() -> string_seq {
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

    tc.link_archive = read_opt(create_archive, [&]() -> string_seq {
        if (!compiler_id) {
            fail(context, "Unable to deduce archive creation rules without a Compiler-ID");
        }
        if (is_msvc) {
            return {"lib", "/nologo", "/OUT:<OUT>", "<IN>"};
        } else if (is_gnu_like) {
            return {"ar", "rcs", "<OUT>", "<IN>"};
        }
        assert(false && "No archive command");
        std::terminate();
    });

    tc.link_exe = read_opt(link_executable, [&]() -> string_seq {
        if (!compiler_id) {
            fail(context, "Unable to deduce how to link executables without a Compiler-ID");
        }
        if (is_msvc) {
            return {get_compiler(true), "/nologo", "/EHsc", "<IN>", "/Fe<OUT>"};
        } else if (is_gnu_like) {
            return {get_compiler(true),
                    "-fPIC",
                    "-fdiagnostics-color",
                    "<IN>",
                    "-pthread",
                    "-lstdc++fs",
                    "-o<OUT>"};
        }
        assert(false && "No link-exe command");
        std::terminate();
    });

    return tc.realize();
}
