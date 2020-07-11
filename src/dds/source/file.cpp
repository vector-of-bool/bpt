#include "./file.hpp"

#include <dds/util/string.hpp>

#include <algorithm>
#include <cassert>
#include <optional>
#include <vector>

using namespace dds;

std::optional<source_kind> dds::infer_source_kind(path_ref p) noexcept {
    static std::vector<std::string_view> header_exts = {
        ".H",
        ".H++",
        ".h",
        ".h++",
        ".hh",
        ".hpp",
        ".hxx",
        ".inc",
        ".inl",
        ".ipp",
    };
    assert(std::is_sorted(header_exts.begin(), header_exts.end()));
    static std::vector<std::string_view> source_exts = {
        ".C",
        ".c",
        ".c++",
        ".cc",
        ".cpp",
        ".cxx",
    };
    assert(std::is_sorted(header_exts.begin(), header_exts.end()));
    assert(std::is_sorted(source_exts.begin(), source_exts.end()));
    auto leaf = p.filename();

    auto ext_found
        = std::lower_bound(header_exts.begin(), header_exts.end(), p.extension(), std::less<>());
    if (ext_found != header_exts.end() && *ext_found == p.extension()) {
        auto stem = p.stem();
        if (stem.extension() == ".config") {
            return source_kind::header_template;
        }
        return source_kind::header;
    }

    ext_found
        = std::lower_bound(source_exts.begin(), source_exts.end(), p.extension(), std::less<>());
    if (ext_found == source_exts.end() || *ext_found != p.extension()) {
        return std::nullopt;
    }

    if (p.stem().extension() == ".test") {
        return source_kind::test;
    }

    if (p.stem().extension() == ".main") {
        return source_kind::app;
    }

    return source_kind::source;
}

std::optional<source_file> source_file::from_path(path_ref path, path_ref base_path) noexcept {
    auto kind = infer_source_kind(path);
    if (!kind.has_value()) {
        return std::nullopt;
    }

    return source_file{path, base_path, *kind};
}
