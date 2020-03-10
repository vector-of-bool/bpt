#pragma once

#include <dds/util/fs.hpp>

#include <optional>
#include <vector>

namespace dds {

enum class source_kind {
    header,
    header_template,
    source,
    test,
    app,
};

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

using source_list = std::vector<source_file>;

}  // namespace dds