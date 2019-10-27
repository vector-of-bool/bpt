#include "./build.hpp"

#include <dds/build/plan/compile_file.hpp>
#include <dds/logging.hpp>
#include <dds/proc.hpp>
#include <dds/project.hpp>
#include <dds/source.hpp>
#include <dds/toolchain.hpp>
#include <dds/usage_reqs.hpp>
#include <dds/util/algo.hpp>
#include <dds/util/string.hpp>
#include <libman/index.hpp>
#include <libman/parse.hpp>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>

using namespace dds;

using namespace ranges;
using namespace ranges::views;

namespace {

struct archive_failure : std::runtime_error {
    using runtime_error::runtime_error;
};

/**
 * Return an iterable over every library in the project
 */
auto iter_libraries(const project& pr) {
    std::vector<std::reference_wrapper<const library>> libs;
    if (pr.main_library()) {
        libs.push_back(*pr.main_library());
    }
    extend(libs, pr.submodules());
    return libs;
}

fs::path lib_archive_path(const build_params& params, const library& lib) {
    return params.out_root
        / (fmt::format("lib{}{}", lib.manifest().name, params.toolchain.archive_suffix()));
}

void copy_headers(const fs::path& source, const fs::path& dest, const source_list& sources) {
    for (auto& file : sources) {
        if (file.kind != source_kind::header) {
            continue;
        }
        auto relpath    = fs::relative(file.path, source);
        auto dest_fpath = dest / relpath;
        spdlog::info("Export header: {}", relpath.string());
        fs::create_directories(dest_fpath.parent_path());
        fs::copy_file(file.path, dest_fpath);
    }
}

fs::path export_project_library(const build_params& params,
                                const library&      lib,
                                const project&      project,
                                path_ref            export_root) {
    auto relpath      = fs::relative(lib.path(), project.root());
    auto lib_out_root = export_root / relpath;
    auto header_root  = lib.path() / "include";
    if (!fs::is_directory(header_root)) {
        header_root = lib.path() / "src";
    }

    auto lml_path       = export_root / fmt::format("{}.lml", lib.manifest().name);
    auto lml_parent_dir = lml_path.parent_path();

    std::vector<lm::pair> pairs;
    pairs.emplace_back("Type", "Library");
    pairs.emplace_back("Name", lib.manifest().name);

    if (fs::is_directory(header_root)) {
        auto header_dest = lib_out_root / "include";
        copy_headers(header_root, header_dest, lib.all_sources());
        pairs.emplace_back("Include-Path", fs::relative(header_dest, lml_parent_dir).string());
    }

    auto ar_path = lib_archive_path(params, lib);
    if (fs::is_regular_file(ar_path)) {
        // XXX: We don't want to just export the archive simply because it exists!
        //      We should check that we actually generated one as part of the
        //      build, otherwise we might end up packaging a random static lib.
        auto ar_dest = lib_out_root / ar_path.filename();
        fs::copy_file(ar_path, ar_dest);
        pairs.emplace_back("Path", fs::relative(ar_dest, lml_parent_dir).string());
    }

    for (const auto& use : lib.manifest().uses) {
        pairs.emplace_back("Uses", fmt::format("{}/{}", use.namespace_, use.name));
    }
    for (const auto& links : lib.manifest().links) {
        pairs.emplace_back("Links", fmt::format("{}/{}", links.namespace_, links.name));
    }

    lm::write_pairs(lml_path, pairs);
    return lml_path;
}

void export_project(const build_params& params, const project& project) {
    if (project.manifest().name.empty()) {
        throw compile_failure(
            fmt::format("Cannot generate an export when the project has no name (Provide a "
                        "package.dds with a `Name` field)"));
    }
    const auto export_root = params.out_root / fmt::format("{}.lpk", project.manifest().name);
    spdlog::info("Generating project export: {}", export_root.string());
    fs::remove_all(export_root);
    fs::create_directories(export_root);

    std::vector<lm::pair> pairs;

    pairs.emplace_back("Type", "Package");
    pairs.emplace_back("Name", project.manifest().name);
    pairs.emplace_back("Namespace", project.manifest().name);

    auto all_libs = iter_libraries(project);
    extend(pairs,
           all_libs  //
               | transform([&](auto&& lib) {
                     return export_project_library(params, lib, project, export_root);
                 })                                                                           //
               | transform([](auto&& path) { return lm::pair("Library", path.string()); }));  //

    lm::write_pairs(export_root / "package.lmp", pairs);
}


usage_requirement_map
load_usage_requirements(path_ref project_root, path_ref build_root, path_ref user_lm_index) {
    fs::path lm_index_path = user_lm_index;
    for (auto cand : {project_root / "INDEX.lmi", build_root / "INDEX.lmi"}) {
        if (fs::exists(lm_index_path) || !user_lm_index.empty()) {
            break;
        }
        lm_index_path = cand;
    }
    if (!fs::exists(lm_index_path)) {
        spdlog::warn("No INDEX.lmi found, so we won't be able to load/use any dependencies");
        return {};
    }
    lm::index idx = lm::index::from_file(lm_index_path);
    return usage_requirement_map::from_lm_index(idx);
}

}  // namespace

void dds::build(const build_params& params, const package_manifest& man) {
    auto libs = collect_libraries(params.root);
    if (!libs.size()) {
        spdlog::warn("Nothing found to build!");
        return;
    }

    build_plan plan;
    auto&      pkg = plan.add_package(package_plan(man.name, man.namespace_));

    usage_requirement_map ureqs
        = load_usage_requirements(params.root, params.out_root, params.lm_index);

    library_build_params lib_params;
    lib_params.build_tests = params.build_tests;
    lib_params.build_apps  = params.build_apps;
    for (const library& lib : libs) {
        lib_params.out_subdir = fs::relative(lib.path(), params.root);
        pkg.add_library(library_plan::create(lib, lib_params, ureqs));
    }

    dds::build_env env{params.toolchain, params.out_root};
    plan.compile_all(env, params.parallel_jobs);
    plan.archive_all(env, params.parallel_jobs);
    plan.link_all(env, params.parallel_jobs);

    auto project = project::from_directory(params.root);

    // dds::execute_all(compiles, params.parallel_jobs, env);

    using namespace ranges::views;

    // auto link_res = link_project(params, project, compiles);

    // auto all_tests = link_res                                    //
    //     | transform([](auto&& link) { return link.test_exes; })  //
    //     | ranges::actions::join;

    // int n_test_fails = 0;
    // for (path_ref test_exe : all_tests) {
    //     spdlog::info("Running test: {}", fs::relative(test_exe, params.out_root).string());
    //     const auto test_res = run_proc({test_exe.string()});
    //     if (!test_res.okay()) {
    //         spdlog::error("TEST FAILED\n{}", test_res.output);
    //         n_test_fails++;
    //     }
    // }

    // if (n_test_fails) {
    //     throw compile_failure("Test failures during build");
    // }

    // if (params.do_export) {
    //     export_project(params, project);
    // }
}
