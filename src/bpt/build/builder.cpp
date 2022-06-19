#include "./builder.hpp"

#include <bpt/build/plan/compile_exec.hpp>
#include <bpt/build/plan/full.hpp>
#include <bpt/compdb.hpp>
#include <bpt/error/doc_ref.hpp>
#include <bpt/error/errors.hpp>
#include <bpt/usage_reqs.hpp>
#include <bpt/util/fs/io.hpp>
#include <bpt/util/fs/path.hpp>
#include <bpt/util/log.hpp>
#include <bpt/util/output.hpp>
#include <bpt/util/time.hpp>

#include <boost/leaf/exception.hpp>
#include <fansi/styled.hpp>
#include <fmt/ostream.h>
#include <neo/out.hpp>
#include <neo/ranges.hpp>
#include <nlohmann/json.hpp>
#include <range/v3/algorithm/contains.hpp>

#include <array>
#include <fstream>
#include <set>

using namespace bpt;
using namespace fansi::literals;

using json = nlohmann::ordered_json;

namespace {

void log_failure(const test_failure& fail) {
    bpt_log(error,
            "Test .br.yellow[{}] .br.red[{}] [Exited {}]"_styled,
            fail.executable_path.string(),
            fail.timed_out ? "TIMED OUT" : "FAILED",
            fail.retc);
    if (fail.signal) {
        bpt_log(error, "Test execution received signal {}", fail.signal);
    }
    if (trim_view(fail.output).empty()) {
        bpt_log(error, "(Test executable produced no output)");
    } else {
        bpt_log(error, "Test output:\n{}[bpt - test output end]", fail.output);
    }
}

///? XXX: This library activating method is a poor hack to ensure only libraries that are used by
///? the main libraries are actually built.
/// TODO: Make this less ugly.
struct lib_prep_info {
    const sdist_target&      sdt;
    const crs::library_info& lib;
    const crs::package_info& pkg;
};

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

static void activate_more(neo::output<std::set<const lib_prep_info*>> libs_to_build,
                          std::map<lm::usage, lib_prep_info> const&   all_libs,
                          const lib_prep_info&                        inf) {
    auto did_insert = libs_to_build.get().emplace(&inf).second;
    if (not did_insert) {
        return;
    }
    auto add_dep = [&](const crs::dependency& dep) {
        for (bpt::name const& used_name : dep.uses) {
            auto found = all_libs.find(lm::usage{dep.name.str, used_name.str});
            neo_assert(invariant,
                       found != all_libs.end(),
                       "Failed to find a dependency library for build activation",
                       inf.lib.name,
                       dep.name,
                       used_name);
            activate_more(libs_to_build, all_libs, found->second);
        }
    };
    for (crs::dependency const& dep : inf.lib.dependencies) {
        add_dep(dep);
    }
    if (inf.sdt.params.build_tests) {
        for (crs::dependency const& dep : inf.lib.test_dependencies) {
            add_dep(dep);
        }
    }
}

build_plan prepare_build_plan(neo::ranges::range_of<sdist_target> auto&& sdists) {
    build_plan plan;
    // First generate a mapping of all libraries
    std::map<lm::usage, lib_prep_info> all_libs;
    // Keep track of which libraries we want to build:
    std::set<const lib_prep_info*> libs_to_build;
    std::set<const lib_prep_info*> leaf_libs_to_build;
    // Iterate all loaded sdists:
    for (const sdist_target& sdt : sdists) {
        for (const auto& lib : sdt.sd.pkg.libraries) {
            const lib_prep_info& lpi =  //
                all_libs
                    .emplace(lm::usage(sdt.sd.pkg.id.name.str, lib.name.str),
                             lib_prep_info{
                                 .sdt = sdt,
                                 .lib = lib,
                                 .pkg = sdt.sd.pkg,
                             })
                    .first->second;
            // If the sdist_build_params wants libraries, activate them now:
            if (ranges::contains(sdt.params.build_libraries, lib.name)) {
                leaf_libs_to_build.emplace(&lpi);
            }
        }
    }
    for (auto lib : leaf_libs_to_build) {
        activate_more(neo::into(libs_to_build), all_libs, *lib);
    }
    // Create package plans for each library and the package it owns:
    std::map<bpt::name, package_plan> pkg_plans;
    for (auto lp : libs_to_build) {
        package_plan pkg{lp->sdt.sd.pkg.id.name.str};
        auto         cur = pkg_plans.find(lp->sdt.sd.pkg.id.name);
        if (cur == pkg_plans.end()) {
            cur = pkg_plans
                      .emplace(lp->sdt.sd.pkg.id.name, package_plan{lp->sdt.sd.pkg.id.name.str})
                      .first;
        }
        cur->second.add_library(prepare_library(lp->sdt, lp->lib, lp->pkg));
    }
    // Add all the packages to the plan:
    for (const auto& pair : pkg_plans) {
        plan.add_package(std::move(pair.second));
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

json get_built_lib(build_env_ref env, const library_plan& lib) {
    auto ret = json::object();
    ret.emplace("name", std::string(lib.name()));
    if (auto const& ar = lib.archive_plan(); ar.has_value()) {
        ret.emplace("path",
                    normalize_path(env.output_root / ar->calc_archive_file_path(env.toolchain))
                        .generic_string());
    }
    ret.emplace("include-path", normalize_path(lib.public_include_dir()).generic_string());
    auto uses = json::array();
    for (auto&& use : lib.lib_uses()) {
        uses.push_back(json::object({
            {"package", use.namespace_},
            {"library", use.name},
        }));
    }
    ret.emplace("uses", std::move(uses));
    return ret;
}

json get_built_pkg(build_env_ref env, const package_plan& pkg) {
    auto ret  = json::object();
    auto libs = json::array();

    for (const auto& lib : pkg.libraries()) {
        auto l = get_built_lib(env, lib);
        libs.push_back(std::move(l));
    }

    ret.emplace("libraries", std::move(libs));
    return ret;
}

void write_built_json(build_env_ref env, const build_plan& plan, path_ref json_path) {
    auto root = json::object({{"version", 1}});
    auto pkgs = json::object();
    fs::create_directories(fs::absolute(json_path).parent_path());
    for (const auto& pkg : plan.packages()) {
        auto p = get_built_pkg(env, pkg);
        pkgs.emplace(pkg.name(), std::move(p));
    }
    root.emplace("packages", std::move(pkgs));
    bpt::write_file(json_path, root.dump(2));
}

void write_lib_cmake(build_env_ref env,
                     std::ostream& out,
                     const package_plan& /* pkg */,
                     const library_plan& lib) {
    fmt::print(out, "# Library {}\n", lib.qualified_name());
    auto cmake_name = fmt::format("{}", bpt::replace(lib.qualified_name(), "/", "::"));
    auto cm_kind    = lib.archive_plan().has_value() ? "STATIC" : "INTERFACE";
    fmt::print(
        out,
        "if(TARGET {0})\n"
        "  get_target_property(bpt_imported {0} bpt_IMPORTED)\n"
        "  if(NOT bpt_imported)\n"
        "    message(WARNING [[A target \"{0}\" is already defined, and not by a bpt import]])\n"
        "  endif()\n"
        "else()\n",
        cmake_name);
    fmt::print(out,
               "  add_library({0} {1} IMPORTED GLOBAL)\n"
               "  set_property(TARGET {0} PROPERTY bpt_IMPORTED TRUE)\n"
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
    auto out = bpt::open_file(cmake_out, std::ios::binary | std::ios::out);
    out << "## This CMake file was generated by `bpt build-deps`. DO NOT EDIT!\n\n";
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
    auto db = database::open(params.out_root / ".bpt.db");

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
        bpt_log(trace,
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
        bpt::stopwatch sw;
        plan.compile_all(env, params.parallel_jobs);
        bpt_log(info, "Compilation completed in {:L}ms", sw.elapsed_ms().count());

        sw.reset();
        plan.archive_all(env, params.parallel_jobs);
        bpt_log(info, "Archiving completed in {:L}ms", sw.elapsed_ms().count());

        sw.reset();
        plan.link_all(env, params.parallel_jobs);
        bpt_log(info, "Runtime binary linking completed in {:L}ms", sw.elapsed_ms().count());

        sw.reset();
        auto test_failures = plan.run_all_tests(env, params.parallel_jobs);
        bpt_log(info, "Test execution finished in {:L}ms", sw.elapsed_ms().count());

        for (auto& fail : test_failures) {
            log_failure(fail);
        }
        if (!test_failures.empty()) {
            BOOST_LEAF_THROW_EXCEPTION(make_user_error<errc::test_failure>(),
                                       test_failures,
                                       BPT_ERR_REF("test-failure"));
        }

        if (params.emit_built_json) {
            write_built_json(env, plan, *params.emit_built_json);
        }

        if (params.emit_cmake) {
            write_cmake(env, plan, *params.emit_cmake);
        }
    });
}
