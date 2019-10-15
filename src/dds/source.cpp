#include "./source.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <optional>
#include <vector>

using namespace dds;

std::optional<source_kind> dds::infer_source_kind(path_ref p) noexcept {
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

std::optional<source_file> source_file::from_path(path_ref path) noexcept {
    auto kind = infer_source_kind(path);
    if (!kind.has_value()) {
        return std::nullopt;
    }

    return source_file{path, *kind};
}

source_list source_file::collect_for_dir(path_ref path) {
    source_list ret;
    for (auto entry : fs::recursive_directory_iterator(path)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        auto sf = source_file::from_path(entry.path());
        if (!sf) {
            spdlog::warn("Couldn't infer a source file kind for file: {}", entry.path().string());
        }
        ret.emplace_back(std::move(*sf));
    }
    return ret;
}

source_list source_file::collect_pf_sources(path_ref path) {
    auto include_dir = path / "include";
    auto src_dir     = path / "src";

    source_list sources;

    if (fs::exists(include_dir)) {
        if (!fs::is_directory(include_dir)) {
            throw std::runtime_error("The `include` at the root of the project is not a directory");
        }
        auto inc_sources = source_file::collect_for_dir(include_dir);
        // Drop any source files we found within `include/`
        erase_if(sources, [&](auto& info) {
            if (info.kind != source_kind::header) {
                spdlog::warn("Source file in `include` will not be compiled: {}",
                             info.path.string());
                return true;
            }
            return false;
        });
        extend(sources, inc_sources);
    }

    if (fs::exists(src_dir)) {
        if (!fs::is_directory(src_dir)) {
            throw std::runtime_error("The `src` at the root of the project is not a directory");
        }
        auto src_sources = source_file::collect_for_dir(src_dir);
        extend(sources, src_sources);
    }

    return sources;
}