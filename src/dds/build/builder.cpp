#include "./builder.hpp"

#include <dds/build/plan/compile_exec.hpp>
#include <dds/build/plan/full.hpp>
#include <dds/catch2_embedded.hpp>
#include <dds/compdb.hpp>
#include <dds/error/errors.hpp>
#include <dds/usage_reqs.hpp>
#include <dds/util/log.hpp>
#include <dds/util/output.hpp>
#include <dds/util/time.hpp>

#include <fansi/styled.hpp>
#include <fmt/ostream.h>

#include <array>
#include <set>

using namespace dds;
using namespace fansi::literals;

namespace {

struct state {
    bool generate_catch2_header = false;
    bool generate_catch2_main   = false;
};

void log_failure(const test_failure& fail) {
    dds_log(error,
            "Test .br.yellow[{}] .br.red[{}] [Exited {}]"_styled,
            fail.executable_path.string(),
            fail.timed_out ? "TIMED OUT" : "FAILED",
            fail.retc);
    if (fail.signal) {
        dds_log(error, "Test execution received signal {}", fail.signal);
    }
    if (trim_view(fail.output).empty()) {
        dds_log(error, "(Test executable produced no output)");
    } else {
        dds_log(error, "Test output:\n{}[dds - test output end]", fail.output);
    }
}

lm::library
prepare_catch2_driver(test_lib test_driver, const build_params& params, build_env_ref env_) {
    fs::path test_include_root = params.out_root / "_catch-2.10.2";

    lm::library ret_lib;
    auto        catch_hpp = test_include_root / "catch2/catch.hpp";
    if (!fs::exists(catch_hpp)) {
        fs::create_directories(catch_hpp.parent_path());
        auto hpp_strm = open(catch_hpp, std::ios::out | std::ios::binary);
        auto c2_str   = detail::catch2_embedded_single_header_str();
        hpp_strm.write(c2_str.data(), c2_str.size());
        hpp_strm.close();
    }
    ret_lib.include_paths.push_back(test_include_root);

    if (test_driver == test_lib::catch_) {
        // Don't compile a library helper
        return ret_lib;
    }

    std::string fname;
    std::string definition;

    if (test_driver == test_lib::catch_main) {
        fname      = "catch-main.cpp";
        definition = "CATCH_CONFIG_MAIN";
    } else {
        assert(false && "Impossible: Invalid `test_driver` for catch library");
        std::terminate();
    }

    shared_compile_file_rules comp_rules;
    comp_rules.defs().push_back(definition);

    auto catch_cpp = test_include_root / "catch2" / fname;
    auto cpp_strm  = open(catch_cpp, std::ios::out | std::ios::binary);
    cpp_strm << "#include \"./catch.hpp\"\n";
    cpp_strm.close();

    auto sf = source_file::from_path(catch_cpp, test_include_root);
    assert(sf.has_value());

    compile_file_plan plan{comp_rules, std::move(*sf), "Catch2", "v1"};

    build_env env2 = env_;
    env2.output_root /= "_test-driver";
    auto obj_file = plan.calc_object_file_path(env2);

    if (!fs::exists(obj_file)) {
        dds_log(info, "Compiling Catch2 test driver (This will only happen once)...");
        compile_all(std::array{plan}, env2, 1);
    }

    ret_lib.linkable_path = obj_file;
    return ret_lib;
}

lm::library
prepare_test_driver(const build_params& params, test_lib test_driver, build_env_ref env) {
    if (test_driver == test_lib::catch_ || test_driver == test_lib::catch_main) {
        return prepare_catch2_driver(test_driver, params, env);
    } else {
        assert(false && "Unreachable");
        std::terminate();
    }
}

library_plan prepare_library(state&                  st,
                             const sdist_target&     sdt,
                             const library_root&     lib,
                             const package_manifest& pkg_man) {
    library_build_params lp;
    lp.out_subdir      = sdt.params.subdir;
    lp.build_apps      = sdt.params.build_apps;
    lp.build_tests     = sdt.params.build_tests;
    lp.enable_warnings = sdt.params.enable_warnings;
    if (lp.build_tests) {
        if (pkg_man.test_driver == test_lib::catch_
            || pkg_man.test_driver == test_lib::catch_main) {
            lp.test_uses.push_back({".dds", "Catch"});
            st.generate_catch2_header = true;
            if (pkg_man.test_driver == test_lib::catch_main) {
                lp.test_uses.push_back({".dds", "Catch-Main"});
                st.generate_catch2_main = true;
            }
        }
    }
    return library_plan::create(lib,
                                std::move(lp),
                                pkg_man.namespace_.str + "/" + lib.manifest().name.str);
}

package_plan prepare_one(state& st, const sdist_target& sd) {
    package_plan pkg{sd.sd.manifest.id.name.str, sd.sd.manifest.namespace_.str};
    auto         libs = collect_libraries(sd.sd.path);
    for (const auto& lib : libs) {
        pkg.add_library(prepare_library(st, sd, lib, sd.sd.manifest));
    }
    return pkg;
}

build_plan prepare_build_plan(state& st, const std::vector<sdist_target>& sdists) {
    build_plan plan;
    for (const auto& sd_target : sdists) {
        plan.add_package(prepare_one(st, sd_target));
    }
    return plan;
}

usage_requirement_map
prepare_ureqs(const build_plan& plan, const toolchain& toolchain, path_ref out_root) {
    usage_requirement_map ureqs;
    for (const auto& pkg : plan.packages()) {
        for (const auto& lib : pkg.libraries()) {
            auto& lib_reqs = ureqs.add(pkg.namespace_(), lib.name().str);
            lib_reqs.include_paths.push_back(lib.library_().public_include_dir());
            lib_reqs.uses  = lib.library_().manifest().uses;
            lib_reqs.links = lib.library_().manifest().links;
            if (const auto& arc = lib.archive_plan()) {
                lib_reqs.linkable_path = out_root / arc->calc_archive_file_path(toolchain);
            }
            auto gen_incdir_opt = lib.generated_include_dir();
            if (gen_incdir_opt) {
                lib_reqs.include_paths.push_back(out_root / *gen_incdir_opt);
            }
        }
    }
    return ureqs;
}

void write_lml(build_env_ref env, const library_plan& lib, path_ref lml_path) {
    fs::create_directories(lml_path.parent_path());
    auto out = open(lml_path, std::ios::binary | std::ios::out);
    out << "Type: Library\n"
        << "Name: " << lib.name().str << '\n'
        << "Include-Path: " << lib.library_().public_include_dir().generic_string() << '\n';
    for (auto&& use : lib.uses()) {
        out << "Uses: " << use.namespace_ << "/" << use.name << '\n';
    }
    for (auto&& link : lib.links()) {
        out << "Links: " << link.namespace_ << "/" << link.name << '\n';
    }
    if (auto&& arc = lib.archive_plan()) {
        out << "Path: "
            << (env.output_root / arc->calc_archive_file_path(env.toolchain)).generic_string()
            << '\n';
    }
}

void write_lmp(build_env_ref env, const package_plan& pkg, path_ref lmp_path) {
    fs::create_directories(lmp_path.parent_path());
    auto out = open(lmp_path, std::ios::binary | std::ios::out);
    out << "Type: Package\n"
        << "Name: " << pkg.name() << '\n'
        << "Namespace: " << pkg.namespace_() << '\n';
    for (const auto& lib : pkg.libraries()) {
        auto lml_path = lmp_path.parent_path() / pkg.namespace_() / (lib.name().str + ".lml");
        write_lml(env, lib, lml_path);
        out << "Library: " << lml_path.generic_string() << '\n';
    }
}

void write_lmi(build_env_ref env, const build_plan& plan, path_ref base_dir, path_ref lmi_path) {
    fs::create_directories(fs::absolute(lmi_path).parent_path());
    auto out = open(lmi_path, std::ios::binary | std::ios::out);
    out << "Type: Index\n";
    for (const auto& pkg : plan.packages()) {
        auto lmp_path = base_dir / "_libman" / (pkg.name() + ".lmp");
        write_lmp(env, pkg, lmp_path);
        out << "Package: " << pkg.name() << "; " << lmp_path.generic_string() << '\n';
    }
}

void write_lib_cmake(build_env_ref       env,
                     std::ostream&       out,
                     const package_plan& pkg,
                     const library_plan& lib) {
    fmt::print(out, "# Library {}/{}\n", pkg.namespace_(), lib.name().str);
    auto cmake_name = fmt::format("{}::{}", pkg.namespace_(), lib.name().str);
    auto cm_kind    = lib.archive_plan().has_value() ? "STATIC" : "INTERFACE";
    fmt::print(
        out,
        "if(TARGET {0})\n"
        "  get_target_property(dds_imported {0} dds_IMPORTED)\n"
        "  if(NOT dds_imported)\n"
        "    message(WARNING [[A target \"{0}\" is already defined, and not by a dds import]])\n"
        "  endif()\n"
        "else()\n",
        cmake_name);
    fmt::print(out,
               "  add_library({0} {1} IMPORTED GLOBAL)\n"
               "  set_property(TARGET {0} PROPERTY dds_IMPORTED TRUE)\n"
               "  set_property(TARGET {0} PROPERTY INTERFACE_INCLUDE_DIRECTORIES [[{2}]])\n",
               cmake_name,
               cm_kind,
               lib.library_().public_include_dir().generic_string());
    for (auto&& use : lib.uses()) {
        fmt::print(out,
                   "  set_property(TARGET {} APPEND PROPERTY INTERFACE_LINK_LIBRARIES {}::{})\n",
                   cmake_name,
                   use.namespace_,
                   use.name);
    }
    for (auto&& link : lib.links()) {
        fmt::print(out,
                   "  set_property(TARGET {} APPEND PROPERTY\n"
                   "               INTERFACE_LINK_LIBRARIES $<LINK_ONLY:{}::{}>)\n",
                   cmake_name,
                   link.namespace_,
                   link.name);
    }
    if (auto& arc = lib.archive_plan()) {
        fmt::print(out,
                   "  set_property(TARGET {} PROPERTY IMPORTED_LOCATION [[{}]])\n",
                   cmake_name,
                   (env.output_root / arc->calc_archive_file_path(env.toolchain)).generic_string());
    }
    fmt::print(out, "endif()\n");
}

void write_cmake_pkg(build_env_ref env, std::ostream& out, const package_plan& pkg) {
    fmt::print(out, "## Imports for {}\n", pkg.name());
    for (auto& lib : pkg.libraries()) {
        write_lib_cmake(env, out, pkg, lib);
    }
    fmt::print(out, "\n");
}

void write_cmake(build_env_ref env, const build_plan& plan, path_ref cmake_out) {
    fs::create_directories(fs::absolute(cmake_out).parent_path());
    auto out = open(cmake_out, std::ios::binary | std::ios::out);
    out << "## This CMake file was generated by `dds build-deps`. DO NOT EDIT!\n\n";
    for (const auto& pkg : plan.packages()) {
        write_cmake_pkg(env, out, pkg);
    }
}

/**
 * @brief Calculate a hash of the directory layout of the given directory.
 *
 * Because a tweaks-dir is specifically designed to have files added/removed within it, and
 * its contents are inspected by `__has_include`, we need to have a way to invalidate any caches
 * when the content of that directory changes. We don't care to hash the contents of the files,
 * since those will already break any caches.
 */
std::string hash_tweaks_dir(const fs::path& tweaks_dir) {
    if (!fs::is_directory(tweaks_dir)) {
        return "0";  // No tweaks directory, no cache to bust
    }
    std::vector<fs::path> children{fs::recursive_directory_iterator{tweaks_dir},
                                   fs::recursive_directory_iterator{}};
    std::sort(children.begin(), children.end());
    // A really simple inline djb2 hash
    std::uint32_t hash = 5381;
    for (auto& p : children) {
        for (std::uint32_t c : fs::weakly_canonical(p).string()) {
            hash = ((hash << 5) + hash) + c;
        }
    }
    return std::to_string(hash);
}

template <typename Func>
void with_build_plan(const build_params&              params,
                     const std::vector<sdist_target>& sdists,
                     Func&&                           fn) {
    fs::create_directories(params.out_root);
    auto db = database::open(params.out_root / ".dds.db");

    state     st;
    auto      plan  = prepare_build_plan(st, sdists);
    auto      ureqs = prepare_ureqs(plan, params.toolchain, params.out_root);
    build_env env{
        params.toolchain,
        params.out_root,
        db,
        toolchain_knobs{
            .is_tty     = stdout_is_a_tty(),
            .tweaks_dir = params.tweaks_dir,
        },
        ureqs,
    };

    if (env.knobs.tweaks_dir) {
        env.knobs.cache_buster = hash_tweaks_dir(*env.knobs.tweaks_dir);
        dds_log(trace,
                "Build cache-buster value for tweaks-dir [{}] content is '{}'",
                *env.knobs.tweaks_dir,
                *env.knobs.cache_buster);
    }

    if (st.generate_catch2_main) {
        auto catch_lib                  = prepare_test_driver(params, test_lib::catch_main, env);
        ureqs.add(".dds", "Catch-Main") = catch_lib;
    }
    if (st.generate_catch2_header) {
        auto catch_lib             = prepare_test_driver(params, test_lib::catch_, env);
        ureqs.add(".dds", "Catch") = catch_lib;
    }

    if (params.generate_compdb) {
        generate_compdb(plan, env);
    }

    plan.render_all(env);

    fn(std::move(env), std::move(plan));
}

}  // namespace

void builder::compile_files(const std::vector<fs::path>& files, const build_params& params) const {
    with_build_plan(params, _sdists, [&](build_env_ref env, build_plan plan) {
        plan.compile_files(env, params.parallel_jobs, files);
    });
}

void builder::build(const build_params& params) const {
    with_build_plan(params, _sdists, [&](build_env_ref env, const build_plan& plan) {
        dds::stopwatch sw;
        plan.compile_all(env, params.parallel_jobs);
        dds_log(info, "Compilation completed in {:L}ms", sw.elapsed_ms().count());

        sw.reset();
        plan.archive_all(env, params.parallel_jobs);
        dds_log(info, "Archiving completed in {:L}ms", sw.elapsed_ms().count());

        sw.reset();
        plan.link_all(env, params.parallel_jobs);
        dds_log(info, "Runtime binary linking completed in {:L}ms", sw.elapsed_ms().count());

        sw.reset();
        auto test_failures = plan.run_all_tests(env, params.parallel_jobs);
        dds_log(info, "Test execution finished in {:L}ms", sw.elapsed_ms().count());

        for (auto& fail : test_failures) {
            log_failure(fail);
        }
        if (!test_failures.empty()) {
            throw_user_error<errc::test_failure>();
        }

        if (params.emit_lmi) {
            write_lmi(env, plan, params.out_root, *params.emit_lmi);
        }

        if (params.emit_cmake) {
            write_cmake(env, plan, *params.emit_cmake);
        }
    });
}
