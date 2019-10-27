#include "./build.hpp"

#include <dds/build/plan/compile_file.hpp>
#include <dds/logging.hpp>
#include <dds/proc.hpp>
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

void copy_headers(const fs::path& source, const fs::path& dest) {
    for (fs::path file : fs::recursive_directory_iterator(source)) {
        if (infer_source_kind(file) != source_kind::header) {
            continue;
        }
        auto relpath    = fs::relative(file, source);
        auto dest_fpath = dest / relpath;
        spdlog::info("Export header: {}", relpath.string());
        fs::create_directories(dest_fpath.parent_path());
        fs::copy_file(file, dest_fpath);
    }
}

fs::path export_project_library(const library_plan& lib, build_env_ref env, path_ref export_root) {
    auto lib_out_root = export_root / lib.name();
    auto header_root  = lib.source_root() / "include";
    if (!fs::is_directory(header_root)) {
        header_root = lib.source_root() / "src";
    }

    auto lml_path       = export_root / fmt::format("{}.lml", lib.name());
    auto lml_parent_dir = lml_path.parent_path();

    std::vector<lm::pair> pairs;
    pairs.emplace_back("Type", "Library");
    pairs.emplace_back("Name", lib.name());

    if (fs::is_directory(header_root)) {
        auto header_dest = lib_out_root / "include";
        copy_headers(header_root, header_dest);
        pairs.emplace_back("Include-Path", fs::relative(header_dest, lml_parent_dir).string());
    }

    if (lib.create_archive()) {
        auto ar_path = lib.create_archive()->calc_archive_file_path(env);
        auto ar_dest = lib_out_root / ar_path.filename();
        fs::copy_file(ar_path, ar_dest);
        pairs.emplace_back("Path", fs::relative(ar_dest, lml_parent_dir).string());
    }

    for (const auto& use : lib.uses()) {
        pairs.emplace_back("Uses", fmt::format("{}/{}", use.namespace_, use.name));
    }
    for (const auto& links : lib.links()) {
        pairs.emplace_back("Links", fmt::format("{}/{}", links.namespace_, links.name));
    }

    lm::write_pairs(lml_path, pairs);
    return lml_path;
}

void export_project(const package_plan& pkg, build_env_ref env) {
    if (pkg.name().empty()) {
        throw compile_failure(
            fmt::format("Cannot generate an export when the package has no name (Provide a "
                        "package.dds with a `Name` field)"));
    }
    const auto export_root = env.output_root / fmt::format("{}.lpk", pkg.name());
    spdlog::info("Generating project export: {}", export_root.string());
    fs::remove_all(export_root);
    fs::create_directories(export_root);

    std::vector<lm::pair> pairs;

    pairs.emplace_back("Type", "Package");
    pairs.emplace_back("Name", pkg.name());
    pairs.emplace_back("Namespace", pkg.namespace_());

    for (const auto& lib : pkg.libraries()) {
        export_project_library(lib, env, export_root);
    }

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
    lib_params.build_tests     = params.build_tests;
    lib_params.build_apps      = params.build_apps;
    lib_params.enable_warnings = params.enable_warnings;
    for (const library& lib : libs) {
        lib_params.out_subdir = fs::relative(lib.path(), params.root);
        pkg.add_library(library_plan::create(lib, lib_params, ureqs));
    }

    dds::build_env env{params.toolchain, params.out_root};
    plan.compile_all(env, params.parallel_jobs);
    plan.archive_all(env, params.parallel_jobs);
    plan.link_all(env, params.parallel_jobs);

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

    if (params.do_export) {
        export_project(pkg, env);
    }
}
