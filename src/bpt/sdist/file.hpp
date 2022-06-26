#pragma once

#include <bpt/util/fs/path.hpp>

#include <optional>
#include <vector>

namespace bpt {

enum class source_kind {
    // Pure header files, e.g. .h
    header,
    // "Header" implementation files which are #included in header files. e.g. .inl
    header_impl,
    source,
    test,
    app,
};

constexpr bool is_header(source_kind kind) {
    return kind == source_kind::header || kind == source_kind::header_impl;
}

std::optional<source_kind> infer_source_kind(path_ref) noexcept;

struct source_file {
    /**
     * The actual path to the file
     */
    fs::path path;
    /**
     * The path to source root that contains the file in question
     */
    fs::path basis_path;
    /**
     * The kind of the source file
     */
    source_kind kind;

    static std::optional<source_file> from_path(path_ref path, path_ref base_path) noexcept;

    fs::path relative_path() const noexcept { return fs::relative(path, basis_path); }
};

}  // namespace bpt
