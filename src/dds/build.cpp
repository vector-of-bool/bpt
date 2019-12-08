#include "./build.hpp"

#include <dds/build/plan/compile_exec.hpp>
#include <dds/catch2_embedded.hpp>
#include <dds/compdb.hpp>
#include <dds/usage_reqs.hpp>
#include <dds/util/algo.hpp>
#include <dds/util/time.hpp>
#include <libman/index.hpp>
#include <libman/parse.hpp>

#include <range/v3/algorithm/transform.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include <array>
#include <map>
#include <set>
#include <stdexcept>

using namespace dds;

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
        fs::create_directories(ar_dest.parent_path());
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

void prepare_catch2_driver(library_build_params& lib_params,
                           test_lib              test_driver,
                           const build_params&   params,
                           build_env_ref         env_) {
    fs::path test_include_root = params.out_root / "_catch-2.10.2";
    lib_params.test_include_dirs.emplace_back(test_include_root);

    auto catch_hpp = test_include_root / "catch2/catch.hpp";
    if (!fs::exists(catch_hpp)) {
        fs::create_directories(catch_hpp.parent_path());
        auto hpp_strm = open(catch_hpp, std::ios::out | std::ios::binary);
        hpp_strm.write(detail::catch2_embedded_single_header_str,
                       std::strlen(detail::catch2_embedded_single_header_str));
        hpp_strm.close();
    }

    if (test_driver == test_lib::catch_) {
        // Don't generate a test library helper
        return;
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
        spdlog::info("Compiling Catch2 test driver (This will only happen once)...");
        compile_all(std::array{plan}, env2, 1);
    }

    lib_params.test_link_files.push_back(obj_file);
}

void prepare_test_driver(library_build_params&   lib_params,
                         const build_params&     params,
                         const package_manifest& man,
                         build_env_ref           env) {
    auto& test_driver = *man.test_driver;
    if (test_driver == test_lib::catch_ || test_driver == test_lib::catch_main) {
        prepare_catch2_driver(lib_params, test_driver, params, env);
    } else {
        assert(false && "Unreachable");
        std::terminate();
    }
}

void add_ureqs(usage_requirement_map& ureqs,
               const sdist&           sd,
               const library&         lib,
               const library_plan&    lib_plan,
               build_env_ref          env) {
    lm::library& reqs = ureqs.add(sd.manifest.namespace_, lib.manifest().name);
    reqs.include_paths.push_back(lib.public_include_dir());
    reqs.name  = lib.manifest().name;
    reqs.uses  = lib.manifest().uses;
    reqs.links = lib.manifest().links;
    if (lib_plan.create_archive()) {
        reqs.linkable_path = lib_plan.create_archive()->calc_archive_file_path(env);
    }
    // TODO: preprocessor definitions
}

using sdist_index_type = std::map<std::string, std::reference_wrapper<const sdist>>;
using sdist_names      = std::set<std::string>;

void add_sdist_to_build(build_plan&             plan,
                        const sdist&            sd,
                        const sdist_index_type& sd_idx,
                        build_env_ref           env,
                        usage_requirement_map&  ureqs,
                        sdist_names&            already_added) {
    if (already_added.find(sd.manifest.pk_id.name) != already_added.end()) {
        // This one has already been added
        return;
    }
    spdlog::debug("Adding dependent build: {}", sd.manifest.pk_id.name);
    // Ensure that ever dependency is loaded up first)
    for (const auto& dep : sd.manifest.dependencies) {
        auto other = sd_idx.find(dep.name);
        assert(other != sd_idx.end()
               && "Failed to load a transitive dependency shortly after initializing them. What?");
        add_sdist_to_build(plan, other->second, sd_idx, env, ureqs, already_added);
    }
    // Record that we have been processed
    already_added.insert(sd.manifest.pk_id.name);
    // Finally, actually add the package:
    auto& pkg  = plan.add_package(package_plan(sd.manifest.pk_id.name, sd.manifest.namespace_));
    auto  libs = collect_libraries(sd.path);
    for (const auto& lib : libs) {
        shared_compile_file_rules comp_rules = lib.base_compile_rules();
        library_build_params      lib_params;
        auto                      lib_plan = library_plan::create(lib, lib_params, ureqs);
        // Create usage requirements for this libary.
        add_ureqs(ureqs, sd, lib, lib_plan, env);
        // Add it to the plan:
        pkg.add_library(std::move(lib_plan));
    }
}

void add_deps_to_build(build_plan&             plan,
                       usage_requirement_map&  ureqs,
                       const build_params&     params,
                       const package_manifest& man,
                       build_env_ref           env) {
    auto sd_idx = params.dep_sdists  //
        | ranges::views::transform([](const auto& sd) {
                      return std::pair(sd.manifest.pk_id.name, std::cref(sd));
                  })  //
        | ranges::to<sdist_index_type>();

    sdist_names already_added;
    for (const sdist& sd : params.dep_sdists) {
        add_sdist_to_build(plan, sd, sd_idx, env, ureqs, already_added);
    }
}

}  // namespace

void dds::build(const build_params& params, const package_manifest& man) {
    fs::create_directories(params.out_root);
    auto           db = database::open(params.out_root / ".dds.db");
    dds::build_env env{params.toolchain, params.out_root, db};

    // The build plan we will fill out:
    build_plan plan;

    // Collect libraries for the current project
    auto libs = collect_libraries(params.root);
    if (!libs.size()) {
        spdlog::warn("Nothing found to build!");
        return;
    }

    usage_requirement_map ureqs;

    if (params.existing_lm_index) {
        ureqs = load_usage_requirements(params.root, params.out_root, *params.existing_lm_index);
    } else {
        add_deps_to_build(plan, ureqs, params, man, env);
    }

    // Initialize the build plan for this project.
    auto& pkg = plan.add_package(package_plan(man.pk_id.name, man.namespace_));

    // assert(false && "Not ready yet!");

    library_build_params lib_params;
    lib_params.build_tests     = params.build_tests;
    lib_params.build_apps      = params.build_apps;
    lib_params.enable_warnings = params.enable_warnings;

    if (man.test_driver) {
        prepare_test_driver(lib_params, params, man, env);
    }

    for (const library& lib : libs) {
        lib_params.out_subdir = fs::relative(lib.path(), params.root);
        pkg.add_library(library_plan::create(lib, lib_params, ureqs));
    }

    if (params.generate_compdb) {
        generate_compdb(plan, env);
    }

    dds::stopwatch sw;
    plan.compile_all(env, params.parallel_jobs);
    spdlog::info("Compilation completed in {:n}ms", sw.elapsed_ms().count());

    sw.reset();
    plan.archive_all(env, params.parallel_jobs);
    spdlog::info("Archiving completed in {:n}ms", sw.elapsed_ms().count());

    if (params.build_apps || params.build_tests) {
        sw.reset();
        plan.link_all(env, params.parallel_jobs);
        spdlog::info("Runtime binary linking completed in {:n}ms", sw.elapsed_ms().count());
    }

    if (params.build_tests) {
        sw.reset();
        auto test_failures = plan.run_all_tests(env, params.parallel_jobs);
        spdlog::info("Test execution finished in {:n}ms", sw.elapsed_ms().count());

        for (auto& failures : test_failures) {
            spdlog::error("Test {} failed! Output:\n{}[dds - test output end]",
                          failures.executable_path.string(),
                          failures.output);
        }
        if (!test_failures.empty()) {
            throw compile_failure("Test failures during the build!");
        }
    }

    if (params.do_export) {
        export_project(pkg, env);
    }
}
