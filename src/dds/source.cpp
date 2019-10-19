#include "./source.hpp"

#include <dds/util/tl.hpp>
#include <dds/util/string.hpp>

#include <spdlog/spdlog.h>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

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

source_list source_file::collect_for_dir(path_ref src) {
    using namespace ranges::view;
    // Strips nullopt elements and lifts the value from the results
    auto drop_nulls =                   //
        filter(DDS_TL(_1.has_value()))  //
        | transform(DDS_TL(*_1));

    // Collect all source files from the directory
    return                                               //
        fs::recursive_directory_iterator(src)            //
        | filter(DDS_TL(_1.is_regular_file()))           //
        | transform(DDS_TL(source_file::from_path(_1)))  //
        // source_file::from_path returns an optional. Drop nulls
        | drop_nulls  //
        | ranges::to_vector;
}
