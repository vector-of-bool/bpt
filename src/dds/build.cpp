#include "./build.hpp"

#include <dds/compile.hpp>
#include <dds/logging.hpp>
#include <dds/proc.hpp>
#include <dds/source.hpp>
#include <dds/toolchain.hpp>
#include <dds/lm_parse.hpp>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>

using namespace dds;

namespace {

struct archive_failure : std::runtime_error {
    using runtime_error::runtime_error;
};

fs::path object_file_path(fs::path source_path, const build_params& params) {
    auto obj_dir     = params.out_root / "obj";
    auto obj_relpath = fs::relative(source_path, params.root);
    obj_relpath.replace_filename(obj_relpath.filename().string()
                                 + params.toolchain.object_suffix());
    auto obj_path = obj_dir / obj_relpath;
    return obj_path;
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

void generate_export(const build_params& params,
                     fs::path            archive_file,
                     const source_list&  sources) {
    const auto export_root = params.out_root / fmt::format("{}.lpk", params.export_name);
    spdlog::info("Generating library export: {}", export_root.string());
    fs::remove_all(export_root);
    fs::create_directories(export_root);
    const auto archive_dest = export_root / archive_file.filename();
    fs::copy_file(archive_file, archive_dest);

    auto       header_root = params.root / "include";
    const auto header_dest = export_root / "include";
    if (!fs::is_directory(header_root)) {
        header_root = params.root / "src";
    }
    if (fs::is_directory(header_root)) {
        copy_headers(header_root, header_dest, sources);
    }

    std::vector<lm_pair> lm_pairs;
    lm_pairs.emplace_back("Type", "Package");
    lm_pairs.emplace_back("Name", params.export_name);
    lm_pairs.emplace_back("Namespace", params.export_name);
    lm_pairs.emplace_back("Library", "lib.lml");
    lm_write_pairs(export_root / "package.lmp", lm_pairs);

    lm_pairs.clear();
    lm_pairs.emplace_back("Type", "Library");
    lm_pairs.emplace_back("Name", params.export_name);
    lm_pairs.emplace_back("Path", fs::relative(archive_dest, export_root).string());
    lm_pairs.emplace_back("Include-Path", fs::relative(header_dest, export_root).string());
    lm_write_pairs(export_root / "lib.lml", lm_pairs);
}

fs::path
link_test(const fs::path& source_file, const build_params& params, const fs::path& lib_archive) {
    const auto obj_file = object_file_path(source_file, params);
    if (!fs::exists(obj_file)) {
        throw compile_failure(
            fmt::format("Unable to find a generated test object file where expected ({})",
                        obj_file.string()));
    }

    const auto    test_name = source_file.stem().stem().string();
    link_exe_spec spec;
    extend(spec.inputs, {obj_file, lib_archive});
    spec.output = params.out_root
        / fs::relative(source_file, params.root)
              .replace_filename(test_name + params.toolchain.executable_suffix());
    const auto link_command = params.toolchain.create_link_executable_command(spec);

    spdlog::info("Linking test executable: {}", spec.output.string());
    fs::create_directories(spec.output.parent_path());
    auto proc_res = run_proc(link_command);
    if (proc_res.retc != 0) {
        throw compile_failure(
            fmt::format("Failed to link test executable '{}'. Link command [{}] returned {}:\n{}",
                        spec.output.string(),
                        quote_command(link_command),
                        proc_res.retc,
                        proc_res.output));
    }

    return spec.output;
}

void link_app(const fs::path&     source_file,
              const build_params& params,
              const fs::path&     lib_archive) {
    const auto obj_file = object_file_path(source_file, params);
    if (!fs::exists(obj_file)) {
        throw compile_failure(
            fmt::format("Unable to find a generated app object file where expected ({})",
                        obj_file.string()));
    }

    const auto    app_name = source_file.stem().stem().string();
    link_exe_spec spec;
    extend(spec.inputs, {obj_file, lib_archive});
    spec.output = params.out_root / (app_name + params.toolchain.executable_suffix() + ".tmp");
    const auto link_command = params.toolchain.create_link_executable_command(spec);

    spdlog::info("Linking application executable: {}", spec.output.string());
    auto proc_res = run_proc(link_command);
    if (proc_res.retc != 0) {
        throw compile_failure(fmt::format(
            "Failed to link application executable '{}'. Link command [{}] returned {}:\n{}",
            spec.output.string(),
            quote_command(link_command),
            proc_res.retc,
            proc_res.output));
    }
    fs::rename(spec.output, spec.output.parent_path() / spec.output.stem());
}

std::vector<fs::path> link_tests(const source_list&  sources,
                                 const build_params& params,
                                 const library_manifest&,
                                 const fs::path& lib_archive) {
    std::vector<fs::path> exes;
    for (const auto& source_file : sources) {
        if (source_file.kind == source_kind::test) {
            exes.push_back(link_test(source_file.path, params, lib_archive));
        }
    }
    return exes;
}

void link_apps(const source_list&  sources,
               const build_params& params,
               const fs::path&     lib_archive) {
    for (const auto& source_file : sources) {
        if (source_file.kind == source_kind::app) {
            link_app(source_file.path, params, lib_archive);
        }
    }
}

dds::compilation_set collect_compiles(const build_params& params, const library_manifest& man) {
    source_list sources           = source_file::collect_pf_sources(params.root);
    const bool  need_compile_deps = params.build_tests || params.build_apps || params.build_deps;
    if (need_compile_deps) {
        spdlog::critical("Dependency resolution isn't done yet");
    }

    if (sources.empty()) {
        spdlog::info("No source files found to compile");
    }

    compilation_set comps;
    for (auto& sf : sources) {
        if (sf.kind == source_kind::header) {
            continue;
        }
        if (sf.kind == source_kind::app && !params.build_apps) {
            continue;
        }
        if (sf.kind == source_kind::test && !params.build_tests) {
            continue;
        }

        compilation_rules rules;
        rules.base_path() = params.root / "src";
        extend(rules.defs(), man.private_defines);
        extend(rules.include_dirs(), man.private_includes);
        rules.include_dirs().push_back(fs::absolute(params.root / "src"));
        rules.include_dirs().push_back(fs::absolute(params.root / "include"));

        const auto obj_path = object_file_path(sf.path, params);
        comps.compilations.push_back(file_compilation{std::move(rules),
                                                      sf.path,
                                                      obj_path,
                                                      params.export_name,
                                                      params.enable_warnings});
    }
    return comps;
}

}  // namespace

void dds::build(const build_params& params, const library_manifest& man) {
    auto include_dir = params.root / "include";
    auto src_dir     = params.root / "src";

    auto compiles = collect_compiles(params, man);

    compiles.execute_all(params.toolchain, params.parallel_jobs);

    source_list sources = source_file::collect_pf_sources(params.root);

    archive_spec arc;
    for (const auto& comp : compiles.compilations) {
        arc.input_files.push_back(comp.obj);
    }
    // arc.input_files = compile_sources(sources, params, man);

    arc.out_path = params.out_root
        / (fmt::format("lib{}{}", params.export_name, params.toolchain.archive_suffix()));

    // Create the static library archive
    spdlog::info("Create archive {}", arc.out_path.string());
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

    // Link any test executables
    std::vector<fs::path> test_exes;
    if (params.build_tests) {
        test_exes = link_tests(sources, params, man, arc.out_path);
    }

    if (params.build_apps) {
        link_apps(sources, params, arc.out_path);
    }

    if (params.do_export) {
        generate_export(params, arc.out_path, sources);
    }

    if (params.build_tests) {
        for (const auto& exe : test_exes) {
            spdlog::info("Running test: {}", fs::relative(exe, params.out_root).string());
            const auto test_res = run_proc({exe.string()});
            if (test_res.retc != 0) {
                spdlog::error("TEST FAILED:\n{}", test_res.output);
            }
        }
        spdlog::info("Test run finished");
    }
}
