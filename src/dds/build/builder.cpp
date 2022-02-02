#include "./builder.hpp"

#include <dds/build/plan/compile_exec.hpp>
#include <dds/build/plan/full.hpp>
#include <dds/compdb.hpp>
#include <dds/error/errors.hpp>
#include <dds/usage_reqs.hpp>
#include <dds/util/fs/io.hpp>
#include <dds/util/fs/path.hpp>
#include <dds/util/log.hpp>
#include <dds/util/output.hpp>
#include <dds/util/time.hpp>

#include <fansi/styled.hpp>
#include <fmt/ostream.h>

#include <array>
#include <fstream>
#include <set>

using namespace dds;
using namespace fansi::literals;

namespace {

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

library_plan prepare_library(const sdist_target&      sdt,
                             const crs::library_info& lib,
                             const crs::package_info& pkg_man) {
    library_build_params lp;
    lp.out_subdir      = normalize_path(sdt.params.subdir / lib.path);
    lp.build_apps      = sdt.params.build_apps;
    lp.build_tests     = sdt.params.build_tests;
    lp.enable_warnings = sdt.params.enable_warnings;
    return library_plan::create(sdt.sd.path, pkg_man, lib, std::move(lp));
}

package_plan prepare_one(const sdist_target& sd) {
    auto&        man = sd.sd.pkg;
    package_plan pkg{man.id.name.str};
    for (auto& lib : man.libraries) {
        pkg.add_library(prepare_library(sd, lib, man));
    }
    return pkg;
}

build_plan prepare_build_plan(const std::vector<sdist_target>& sdists) {
    build_plan plan;
    for (const auto& sd_target : sdists) {
        plan.add_package(prepare_one(sd_target));
    }
    return plan;
}

usage_requirements
prepare_ureqs(const build_plan& plan, const toolchain& toolchain, path_ref out_root) {
    usage_requirement_map ureqs;
    for (const auto& pkg : plan.packages()) {
        for (const auto& lib : pkg.libraries()) {
            auto& lib_reqs = ureqs.add({pkg.name(), std::string(lib.name())});
            lib_reqs.include_paths.push_back(lib.public_include_dir());
            lib_reqs.uses = lib.lib_uses();
            //! lib_reqs.links = lib.library_().manifest().links;
            if (const auto& arc = lib.archive_plan()) {
                lib_reqs.linkable_path = out_root / arc->calc_archive_file_path(toolchain);
            }
        }
    }
    return usage_requirements(std::move(ureqs));
}

void write_lml(build_env_ref env, const library_plan& lib, path_ref lml_path) {
    fs::create_directories(lml_path.parent_path());
    auto out = dds::open_file(lml_path, std::ios::binary | std::ios::out);
    //! out << "Type: Library\n"
    //!     << "Name: " << lib.name().str << '\n'
    //!     << "Include-Path: " << lib.library_().public_include_dir().generic_string() << '\n';
    for (auto&& use : lib.lib_uses()) {
        out << "Uses: " << use.namespace_ << "/" << use.name << '\n';
    }
    //! for (auto&& link : lib.links()) {
    //!     out << "Links: " << link.namespace_ << "/" << link.name << '\n';
    //! }
    if (auto&& arc = lib.archive_plan()) {
        out << "Path: "
            << (env.output_root / arc->calc_archive_file_path(env.toolchain)).generic_string()
            << '\n';
    }
}

void write_lmp(build_env_ref env, const package_plan& pkg, path_ref lmp_path) {
    fs::create_directories(lmp_path.parent_path());
    auto out = dds::open_file(lmp_path, std::ios::binary | std::ios::out);
    out << "Type: Package\n"
        << "Name: " << pkg.name() << '\n'
        << "Namespace: " << pkg.name() << '\n';
    for (const auto& lib : pkg.libraries()) {
        auto lml_path = lmp_path.parent_path() / (lib.qualified_name() + ".lml");
        write_lml(env, lib, lml_path);
        out << "Library: " << lml_path.generic_string() << '\n';
    }
}

void write_lmi(build_env_ref env, const build_plan& plan, path_ref base_dir, path_ref lmi_path) {
    fs::create_directories(fs::absolute(lmi_path).parent_path());
    auto out = dds::open_file(lmi_path, std::ios::binary | std::ios::out);
    out << "Type: Index\n";
    for (const auto& pkg : plan.packages()) {
        auto lmp_path = base_dir / "_libman" / (pkg.name() + ".lmp");
        write_lmp(env, pkg, lmp_path);
        out << "Package: " << pkg.name() << "; " << lmp_path.generic_string() << '\n';
    }
}

void write_lib_cmake(build_env_ref env,
                     std::ostream& out,
                     const package_plan& /* pkg */,
                     const library_plan& lib) {
    fmt::print(out, "# Library {}\n", lib.qualified_name());
    auto cmake_name = fmt::format("{}", dds::replace(lib.qualified_name(), "/", "::"));
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
               lib.public_include_dir().generic_string());
    for (auto&& use : lib.lib_uses()) {
        fmt::print(out,
                   "  set_property(TARGET {} APPEND PROPERTY INTERFACE_LINK_LIBRARIES {}::{})\n",
                   cmake_name,
                   use.namespace_,
                   use.name);
    }
    //! for (auto&& link : lib.links()) {
    //!     fmt::print(out,
    //!                "  set_property(TARGET {} APPEND PROPERTY\n"
    //!                "               INTERFACE_LINK_LIBRARIES $<LINK_ONLY:{}::{}>)\n",
    //!                cmake_name,
    //!                link.namespace_,
    //!                link.name);
    //! }
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
    auto out = dds::open_file(cmake_out, std::ios::binary | std::ios::out);
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

    auto      plan  = prepare_build_plan(sdists);
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

    if (params.generate_compdb) {
        generate_compdb(plan, env);
    }

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
