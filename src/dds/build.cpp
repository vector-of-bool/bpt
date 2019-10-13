#include "./build.hpp"

#include <dds/logging.hpp>
#include <dds/proc.hpp>
#include <dds/toolchain.hpp>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <stdexcept>

using namespace dds;

namespace {

struct compile_failure : std::runtime_error {
    using runtime_error::runtime_error;
};

struct archive_failure : std::runtime_error {
    using runtime_error::runtime_error;
};

struct source_files {
    std::vector<fs::path> headers;
    std::vector<fs::path> sources;
};

void collect_sources(source_files& sf, const fs::path& source_dir) {
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
    static auto is_header = [&](const fs::path& p) {
        auto found = std::lower_bound(header_exts.begin(),
                                      header_exts.end(),
                                      p.extension(),
                                      std::less<>());
        return found != header_exts.end() && *found == p.extension();
    };
    static auto is_source = [&](const fs::path& p) {
        auto found  = std::lower_bound(source_exts.begin(),
                                      source_exts.end(),
                                      p.extension(),
                                      std::less<>());
        bool is_cpp = found != source_exts.end() && *found == p.extension();
        auto leaf   = p.string();
        return is_cpp && !ends_with(leaf, ".main.cpp") && !ends_with(leaf, ".test.cpp");
    };

    for (auto entry : fs::recursive_directory_iterator(source_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        auto entry_path = entry.path();
        if (is_header(entry_path)) {
            sf.headers.push_back(std::move(entry_path));
        } else if (is_source(entry_path)) {
            sf.sources.push_back(std::move(entry_path));
        }
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

    spdlog::info("Compile file: {}", src_path.string());

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

void copy_headers(const fs::path& source, const fs::path& dest, const source_files& sources) {
    for (auto& header_fpath : sources.headers) {
        auto relpath    = fs::relative(header_fpath, source);
        auto dest_fpath = dest / relpath;
        spdlog::info("Export header: {}", relpath);
        fs::create_directories(dest_fpath.parent_path());
        fs::copy_file(header_fpath, dest_fpath);
    }
}

void generate_export(const build_params& params,
                     fs::path            archive_file,
                     const source_files& sources) {
    const auto export_root = params.out_root / (params.export_name + ".export-root");
    spdlog::info("Generating library export: {}", export_root);
    fs::remove_all(export_root);
    fs::create_directories(export_root);
    fs::copy_file(archive_file, export_root / archive_file.filename());

    auto header_root = params.root / "include";
    if (!fs::is_directory(header_root)) {
        header_root = params.root / "src";
    }
    if (fs::is_directory(header_root)) {
        copy_headers(header_root, export_root / "include", sources);
    }
}

}  // namespace

void dds::build(const build_params& params, const library_manifest& man) {
    auto tc = toolchain::load_from_file(params.toolchain_file);

    auto include_dir = params.root / "include";
    auto src_dir     = params.root / "src";

    source_files files;

    if (fs::exists(include_dir)) {
        if (!fs::is_directory(include_dir)) {
            throw std::runtime_error("The `include` at the root of the project is not a directory");
        }
        collect_sources(files, include_dir);
        for (auto&& sf : files.sources) {
            spdlog::warn("Source file in `include/` will not be compiled: {}", sf);
        }
        // Drop any source files we found within `include/`
        files.sources.clear();
    }

    if (fs::exists(src_dir)) {
        if (!fs::is_directory(src_dir)) {
            throw std::runtime_error("The `src` at the root of the project is not a directory");
        }
        collect_sources(files, src_dir);
    }

    if (files.sources.empty() && files.headers.empty()) {
        spdlog::warn("No source files found to compile/export!");
    }

    archive_spec arc;
    for (auto&& sf : files.sources) {
        arc.input_files.push_back(compile_file(sf, params, tc, man));
    }

    arc.out_path = params.out_root / ("lib" + params.export_name + tc.archive_suffix());
    spdlog::info("Create archive {}", arc.out_path);
    auto ar_cmd = tc.create_archive_command(arc);
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
        generate_export(params, arc.out_path, files);
    }
}
