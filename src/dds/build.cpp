#include "./build.hpp"

#include <dds/compile.hpp>
#include <dds/logging.hpp>
#include <dds/proc.hpp>
#include <dds/project.hpp>
#include <dds/source.hpp>
#include <dds/toolchain.hpp>
#include <libman/index.hpp>
#include <libman/parse.hpp>

#include <range/v3/action/join.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/join.hpp>
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
using namespace ranges::view;

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

fs::path object_file_path(fs::path source_path, const build_params& params) {
    auto obj_dir     = params.out_root / "obj";
    auto obj_relpath = fs::relative(source_path, params.root);
    obj_relpath.replace_filename(obj_relpath.filename().string()
                                 + params.toolchain.object_suffix());
    auto obj_path = obj_dir / obj_relpath;
    return obj_path;
}

fs::path lib_archive_path(const build_params& params, const library& lib) {
    return params.out_root
        / (fmt::format("lib{}{}", lib.name(), params.toolchain.archive_suffix()));
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
    auto relpath      = fs::relative(lib.base_dir(), project.root());
    auto lib_out_root = export_root / relpath;
    auto header_root  = lib.base_dir() / "include";
    if (!fs::is_directory(header_root)) {
        header_root = lib.base_dir() / "src";
    }

    auto lml_path       = export_root / fmt::format("{}.lml", lib.name());
    auto lml_parent_dir = lml_path.parent_path();

    std::vector<lm::pair> pairs;
    pairs.emplace_back("Type", "Library");
    pairs.emplace_back("Name", lib.name());

    if (fs::is_directory(header_root)) {
        auto header_dest = lib_out_root / "include";
        copy_headers(header_root, header_dest, lib.sources());
        pairs.emplace_back("Include-Path", fs::relative(header_dest, lml_parent_dir).string());
    }

    auto ar_path = lib_archive_path(params, lib);
    if (fs::is_regular_file(ar_path)) {
        auto ar_dest = lib_out_root / ar_path.filename();
        fs::copy_file(ar_path, ar_dest);
        pairs.emplace_back("Path", fs::relative(ar_dest, lml_parent_dir).string());
    }

    for (const auto& use : lib.manifest().uses) {
        pairs.emplace_back("Uses", use);
    }
    for (const auto& links : lib.manifest().links) {
        pairs.emplace_back("Links", links);
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
           all_libs                                                                           //
               | transform(DDS_TL(export_project_library(params, _1, project, export_root)))  //
               | transform(DDS_TL(lm::pair("Library", _1.string())))                          //
    );

    lm::write_pairs(export_root / "package.lmp", pairs);
}

void include_deps(const lm::index::library_index& lib_index,
                  std::vector<fs::path>&          includes,
                  std::vector<std::string>&       defines,
                  std::string_view                usage_key,
                  bool                            is_public_usage) {
    auto pair = split(usage_key, "/");
    if (pair.size() != 2) {
        throw compile_failure(fmt::format("Invalid `Uses`: {}", usage_key));
    }

    auto& pkg_ns = pair[0];
    auto& lib    = pair[1];

    auto found = lib_index.find(std::pair(pkg_ns, lib));
    if (found == lib_index.end()) {
        throw compile_failure(
            fmt::format("No library '{}/{}': Check that it is installed and available",
                        pkg_ns,
                        lib));
    }

    const lm::library& lm_lib = found->second;
    extend(includes, lm_lib.include_paths);
    extend(defines, lm_lib.preproc_defs);

    for (const auto& uses : lm_lib.uses) {
        include_deps(lib_index, includes, defines, uses, is_public_usage && true);
    }
    for (const auto& links : lm_lib.links) {
        include_deps(lib_index, includes, defines, links, false);
    }
}

std::vector<file_compilation> file_compilations_of_lib(const build_params& params,
                                                       const library&      lib) {
    const auto& sources = lib.sources();

    std::vector<fs::path>    dep_includes;
    std::vector<std::string> dep_defines;

    if (!lib.manifest().uses.empty() || !lib.manifest().links.empty()) {
        fs::path lm_index_path = params.lm_index;
        for (auto cand : {"INDEX.lmi", "_build/INDEX.lmi"}) {
            if (!lm_index_path.empty()) {
                break;
            }
            lm_index_path = params.root / cand;
        }
        if (!fs::exists(lm_index_path)) {
            throw compile_failure(
                "No `INDEX.lmi` found, but we need to pull in dependencies."
                "Use a package manager to generate an INDEX.lmi");
        }
        auto lm_index  = lm::index::from_file(lm_index_path);
        auto lib_index = lm_index.build_library_index();

        for (const auto& uses : lib.manifest().uses) {
            include_deps(lib_index, dep_includes, dep_defines, uses, true);
        }
        for (const auto& links : lib.manifest().links) {
            include_deps(lib_index, dep_includes, dep_defines, links, false);
        }
        spdlog::critical("Dependency resolution isn't fully implemented yet!!");
    }

    if (sources.empty()) {
        spdlog::info("No source files found to compile");
    }

    auto should_compile_source = [&](const source_file& sf) {
        return (sf.kind == source_kind::source || (sf.kind == source_kind::app && params.build_apps)
                || (sf.kind == source_kind::test && params.build_tests));
    };

    compilation_rules rules;
    rules.base_path() = lib.base_dir() / "src";
    extend(rules.defs(), lib.manifest().private_defines);
    extend(rules.defs(), dep_defines);
    extend(rules.include_dirs(), lib.manifest().private_includes);
    extend(rules.include_dirs(), dep_includes);
    rules.include_dirs().push_back(fs::absolute(lib.base_dir() / "src"));
    rules.include_dirs().push_back(fs::absolute(lib.base_dir() / "include"));

    return                               //
        sources                          //
        | filter(should_compile_source)  //
        | transform(DDS_TL(file_compilation{rules,
                                            _1,
                                            object_file_path(_1.path, params),
                                            lib.name(),
                                            params.enable_warnings}))  //
        | to_vector;
}

std::vector<dds::file_compilation> collect_compiles(const build_params& params,
                                                    const project&      project) {
    auto libs = iter_libraries(project);
    return                                                         //
        libs                                                       //
        | transform(DDS_TL(file_compilations_of_lib(params, _1)))  //
        | ranges::actions::join                                    //
        | to_vector                                                //
        ;
}

using object_file_index = std::map<fs::path, fs::path>;

/**
 * Obtain the path to the object file that corresponds to the named source file
 */
fs::path obj_for_source(const object_file_index& idx, path_ref source_path) {
    auto iter = idx.find(source_path);
    if (iter == idx.end()) {
        assert(false && "Lookup on invalid source file");
        std::terminate();
    }
    return iter->second;
}

/**
 * Create the static library archive for the given library object.
 */
std::optional<fs::path> create_lib_archive(const build_params&      params,
                                           const library&           lib,
                                           const object_file_index& obj_idx) {
    archive_spec arc;
    arc.out_path = lib_archive_path(params, lib);

    // Collect object files that make up that library
    arc.input_files =                                          //
        lib.sources()                                          //
        | filter(DDS_TL(_1.kind == source_kind::source))       //
        | transform(DDS_TL(obj_for_source(obj_idx, _1.path)))  //
        | to_vector                                            //
        ;

    if (arc.input_files.empty()) {
        return std::nullopt;
    }

    spdlog::info("Create archive for {}: {}", lib.name(), arc.out_path.string());
    auto ar_cmd = params.toolchain.create_archive_command(arc);
    if (fs::exists(arc.out_path)) {
        fs::remove(arc.out_path);
    }
    fs::create_directories(arc.out_path.parent_path());
    auto ar_res = run_proc(ar_cmd);
    if (!ar_res.okay()) {
        spdlog::error("Failure creating archive library {}", arc.out_path);
        spdlog::error("Subcommand failed: {}", quote_command(ar_cmd));
        spdlog::error("Subcommand produced output:\n{}", ar_res.output);
        throw archive_failure("Failed to create the library archive");
    }

    return arc.out_path;
}

/**
 * Link a single test executable identified by a single source file
 */
fs::path link_one_exe(path_ref                 dest,
                      path_ref                 source_file,
                      const build_params&      params,
                      const library&           lib,
                      const object_file_index& obj_idx) {
    auto main_obj = obj_for_source(obj_idx, source_file);
    assert(fs::exists(main_obj));

    link_exe_spec spec;
    spec.inputs.push_back(main_obj);
    auto lib_arc_path = lib_archive_path(params, lib);
    if (fs::is_regular_file(lib_arc_path)) {
        spec.inputs.push_back(lib_arc_path);
    }
    spec.output             = dest;
    const auto link_command = params.toolchain.create_link_executable_command(spec);

    spdlog::info("Create executable: {}", (fs::relative(spec.output, params.out_root)).string());
    fs::create_directories(spec.output.parent_path());
    auto proc_res = run_proc(link_command);
    if (!proc_res.okay()) {
        throw compile_failure(
            fmt::format("Failed to link test executable '{}'. Link command [{}] returned {}:\n{}",
                        spec.output.string(),
                        quote_command(link_command),
                        proc_res.retc,
                        proc_res.output));
    }

    return spec.output;
}

template <typename GetExeNameFn>
std::vector<fs::path> link_executables(source_kind              sk,
                                       GetExeNameFn&&           get_exe_path,
                                       const build_params&      params,
                                       const library&           lib,
                                       const object_file_index& obj_idx) {
    return                                                                                  //
        lib.sources()                                                                       //
        | filter(DDS_TL(_1.kind == sk))                                                     //
        | transform(DDS_TL(link_one_exe(get_exe_path(_1), _1.path, params, lib, obj_idx)))  //
        | to_vector                                                                         //
        ;
}

struct link_results {
    std::optional<fs::path> archive_path;
    std::vector<fs::path>   test_exes;
    std::vector<fs::path>   app_exes;
};

link_results
link_project_lib(const build_params& params, const library& lib, const object_file_index& obj_idx) {
    link_results res;
    auto         op_arc_path = create_lib_archive(params, lib, obj_idx);
    if (op_arc_path) {
        res.archive_path = *op_arc_path;
    }

    auto get_test_exe_path = [&](const source_file sf) {
        return params.out_root
            / fs::relative(sf.path, params.root)
                  .replace_filename(sf.path.stem().stem().string()
                                    + params.toolchain.executable_suffix());
    };

    auto get_app_exe_path = [&](const source_file& sf) {
        return params.out_root
            / (sf.path.stem().stem().string() + params.toolchain.executable_suffix());
    };

    // Link test executables
    if (params.build_tests) {
        extend(res.test_exes,
               link_executables(source_kind::test, get_test_exe_path, params, lib, obj_idx));
    }

    if (params.build_apps) {
        extend(res.app_exes,
               link_executables(source_kind::app, get_app_exe_path, params, lib, obj_idx));
    }

    return res;
}

std::vector<link_results> link_project(const build_params&                  params,
                                       const project&                       pr,
                                       const std::vector<file_compilation>& compilations) {
    auto obj_index =                                            //
        compilations                                            //
        | transform(DDS_TL(std::pair(_1.source.path, _1.obj)))  //
        | to<object_file_index>                                 //
        ;

    auto libs = iter_libraries(pr);
    return libs                                                       //
        | transform(DDS_TL(link_project_lib(params, _1, obj_index)))  //
        | to_vector;
}

}  // namespace

void dds::build(const build_params& params, const package_manifest&) {
    auto project = project::from_directory(params.root);

    auto compiles = collect_compiles(params, project);

    dds::execute_all(compiles, params.toolchain, params.parallel_jobs);

    using namespace ranges::view;

    auto link_res = link_project(params, project, compiles);

    auto all_tests = link_res              //
        | transform(DDS_TL(_1.test_exes))  //
        | action::join;

    int n_test_fails = 0;
    for (path_ref test_exe : all_tests) {
        spdlog::info("Running test: {}", fs::relative(test_exe, params.out_root).string());
        const auto test_res = run_proc({test_exe.string()});
        if (!test_res.okay()) {
            spdlog::error("TEST FAILED\n{}", test_res.output);
            n_test_fails++;
        }
    }

    if (n_test_fails) {
        throw compile_failure("Test failures during build");
    }

    if (params.do_export) {
        export_project(params, project);
    }
}
