#pragma once

#include <dds/util/fs.hpp>

#include <optional>

namespace dds {

enum class source_kind {
    header,
    source,
    test,
    app,
};

std::optional<source_kind> infer_source_kind(path_ref) noexcept;

struct source_file {
    fs::path    path;
    fs::path    basis_path;
    source_kind kind;

    static std::optional<source_file> from_path(path_ref path, path_ref base_path) noexcept;
};

using source_list = std::vector<source_file>;

}  // namespace dds