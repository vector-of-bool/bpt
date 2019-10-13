#include "./build.hpp"

#include <dds/lm_parse.hpp>
#include <dds/logging.hpp>
#include <dds/proc.hpp>
#include <dds/toolchain.hpp>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <mutex>
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

fs::path compile_file(fs::path                src_path,
                      const build_params&     params,
                      const toolchain&        tc,
                      const library_manifest& man) {
    auto obj_dir     = params.out_root / "obj";
    auto obj_relpath = fs::relative(src_path, params.root);
    obj_relpath.replace_filename(obj_relpath.filename().string() + ".o");
    auto obj_path = obj_dir / obj_relpath;
    fs::create_directories(obj_path.parent_path());

    spdlog::info("Compile file: {}", fs::relative(src_path, params.root).string());

    compile_file_spec spec{src_path, obj_path};

    spec.include_dirs.push_back(params.root / "src");
    spec.include_dirs.push_back(params.root / "include");

    for (auto& inc : man.private_includes) {
        spec.include_dirs.push_back(inc);
    }

    for (auto& def : man.private_defines) {
        spec.definitions.push_back(def);
    }

    auto cmd         = tc.create_compile_command(spec);
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
    const auto export_root = params.out_root / (params.export_name + ".export-root");
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

std::vector<fs::path> compile_sources(source_list             sources,
                                      const build_params&     params,
                                      const toolchain&        tc,
                                      const library_manifest& man) {
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
                auto obj_path = compile_file(source.path, params, tc, man);
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
    std::generate_n(std::back_inserter(threads), std::thread::hardware_concurrency() + 2, [&] {
        return std::thread(compile_one);
    });
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
    auto tc = toolchain::load_from_file(params.toolchain_file);

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
    arc.input_files = compile_sources(sources, params, tc, man);

    arc.out_path = params.out_root / ("lib" + params.export_name + tc.archive_suffix());

    spdlog::info("Create archive {}", arc.out_path.string());
    auto ar_cmd = tc.create_archive_command(arc);
    if (fs::exists(arc.out_path)) {
        fs::remove(arc.out_path);
    }
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

    if (params.do_export) {
        generate_export(params, arc.out_path, sources);
    }
}
