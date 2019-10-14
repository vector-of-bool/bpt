#include "./build.hpp"

#include <dds/lm_parse.hpp>
#include <dds/logging.hpp>
#include <dds/proc.hpp>
#include <dds/toolchain.hpp>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>

using namespace dds;

namespace {

struct compile_failure : std::runtime_error {
    using runtime_error::runtime_error;
};

struct archive_failure : std::runtime_error {
    using runtime_error::runtime_error;
};

enum class source_kind {
    header,
    source,
    test,
    app,
};

struct source_file_info {
    fs::path    path;
    source_kind kind;
};

using source_list = std::vector<source_file_info>;

std::optional<source_kind> infer_kind(const fs::path& p) {
    static std::vector<std::string_view> header_exts = {
        ".h",
        ".H",
        ".H++",
        ".h++",
        ".hh",
        ".hpp",
        ".hxx",
        ".inl",
    };
    static std::vector<std::string_view> source_exts = {
        ".C",
        ".c",
        ".c++",
        ".cc",
        ".cpp",
        ".cxx",
    };
    auto leaf = p.filename();

    auto ext_found
        = std::lower_bound(header_exts.begin(), header_exts.end(), p.extension(), std::less<>());
    if (ext_found != header_exts.end() && *ext_found == p.extension()) {
        return source_kind::header;
    }

    ext_found
        = std::lower_bound(source_exts.begin(), source_exts.end(), p.extension(), std::less<>());
    if (ext_found == source_exts.end() || *ext_found != p.extension()) {
        return std::nullopt;
    }

    if (ends_with(p.stem().string(), ".test")) {
        return source_kind::test;
    }

    if (ends_with(p.stem().string(), ".main")) {
        return source_kind::app;
    }

    return source_kind::source;
}

void collect_sources(source_list& sf, const fs::path& source_dir) {
    for (auto entry : fs::recursive_directory_iterator(source_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        auto entry_path = entry.path();
        auto kind       = infer_kind(entry_path);
        if (!kind.has_value()) {
            spdlog::warn("Couldn't infer a source file kind for file: {}", entry_path.string());
            continue;
        }
        source_file_info info{entry_path, *kind};
        sf.emplace_back(std::move(info));
    }
}

fs::path object_file_path(fs::path source_path, const build_params& params) {
    auto obj_dir     = params.out_root / "obj";
    auto obj_relpath = fs::relative(source_path, params.root);
    obj_relpath.replace_filename(obj_relpath.filename().string()
                                 + params.toolchain.object_suffix());
    auto obj_path = obj_dir / obj_relpath;
    return obj_path;
}

fs::path compile_file(fs::path src_path, const build_params& params, const library_manifest& man) {
    auto obj_path = object_file_path(src_path, params);
    fs::create_directories(obj_path.parent_path());

    spdlog::info("Compile file: {}", fs::relative(src_path, params.root).string());

    compile_file_spec spec{src_path, obj_path};
    spec.enable_warnings = params.enable_warnings;

    spec.include_dirs.push_back(params.root / "src");
    spec.include_dirs.push_back(params.root / "include");

    for (auto& inc : man.private_includes) {
        spec.include_dirs.push_back(inc);
    }

    for (auto& def : man.private_defines) {
        spec.definitions.push_back(def);
    }

    auto cmd         = params.toolchain.create_compile_command(spec);
    auto compile_res = run_proc(cmd);
    if (!compile_res.okay()) {
        spdlog::error("Compilation failed: {}", spec.source_path.string());
        std::stringstream strm;
        for (auto& arg : cmd) {
            strm << std::quoted(arg) << ' ';
        }
        spdlog::error("Subcommand FAILED: {}\n{}", strm.str(), compile_res.output);
        throw compile_failure("Compilation failed.");
    }

    // MSVC prints the filename of the source file. Dunno why, but they do.
    if (compile_res.output.find(spec.source_path.filename().string() + "\r\n") == 0) {
        compile_res.output.erase(0, spec.source_path.filename().string().length() + 2);
    }

    if (!compile_res.output.empty()) {
        spdlog::warn("While compiling file {}:\n{}", spec.source_path.string(), compile_res.output);
    }

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
            fmt::format("Failed to link executable '{}'. Link command exited {}:\n{}",
                        spec.output.string(),
                        proc_res.retc,
                        proc_res.output));
    }

    return spec.output;
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

std::vector<fs::path>
compile_sources(source_list sources, const build_params& params, const library_manifest& man) {
    // We don't bother with a nice thread pool, as the overhead of compiling
    // source files dwarfs the cost of interlocking.
    std::mutex                      mut;
    std::atomic_bool                any_error{false};
    std::vector<std::exception_ptr> exceptions;
    std::vector<fs::path>           objects;

    auto compile_one = [&]() mutable {
        while (true) {
            std::unique_lock lk{mut};
            if (!exceptions.empty()) {
                break;
            }
            if (sources.empty()) {
                break;
            }
            auto source = sources.back();
            sources.pop_back();
            if (source.kind == source_kind::header || source.kind == source_kind::app) {
                continue;
            }
            if (source.kind == source_kind::test && !params.build_tests) {
                continue;
            }
            lk.unlock();
            try {
                auto obj_path = compile_file(source.path, params, man);
                lk.lock();
                objects.emplace_back(std::move(obj_path));
            } catch (...) {
                lk.lock();
                exceptions.push_back(std::current_exception());
                break;
            }
        }
    };

    std::unique_lock         lk{mut};
    std::vector<std::thread> threads;
    int                      njobs = params.parallel_jobs;
    if (njobs < 1) {
        njobs = std::thread::hardware_concurrency() + 2;
    }
    std::generate_n(std::back_inserter(threads), njobs, [&] { return std::thread(compile_one); });
    spdlog::info("Parallel compile with {} threads", threads.size());
    lk.unlock();
    for (auto& t : threads) {
        t.join();
    }
    for (auto eptr : exceptions) {
        try {
            std::rethrow_exception(eptr);
        } catch (const std::exception& e) {
            spdlog::error(e.what());
        }
    }
    if (!exceptions.empty()) {
        throw compile_failure("Failed to compile library sources");
    }

    return objects;
}

}  // namespace

void dds::build(const build_params& params, const library_manifest& man) {
    auto include_dir = params.root / "include";
    auto src_dir     = params.root / "src";

    source_list sources;

    if (fs::exists(include_dir)) {
        if (!fs::is_directory(include_dir)) {
            throw std::runtime_error("The `include` at the root of the project is not a directory");
        }
        collect_sources(sources, include_dir);
        // Drop any source files we found within `include/`
        erase_if(sources, [&](auto& info) {
            if (info.kind != source_kind::header) {
                spdlog::warn("Source file in `include` will not be compiled: {}", info.path);
                return true;
            }
            return false;
        });
    }

    if (fs::exists(src_dir)) {
        if (!fs::is_directory(src_dir)) {
            throw std::runtime_error("The `src` at the root of the project is not a directory");
        }
        collect_sources(sources, src_dir);
    }

    if (sources.empty()) {
        spdlog::warn("No source files found to compile/export!");
    }

    archive_spec arc;
    arc.input_files = compile_sources(sources, params, man);

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
        std::stringstream strm;
        for (auto& arg : ar_cmd) {
            strm << std::quoted(arg) << ' ';
        }
        spdlog::error("Subcommand failed: {}", strm.str());
        spdlog::error("Subcommand produced output:\n{}", ar_res.output);
        throw archive_failure("Failed to create the library archive");
    }

    // Link any test executables
    std::vector<fs::path> test_exes;
    if (params.build_tests) {
        test_exes = link_tests(sources, params, man, arc.out_path);
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
