#pragma once

#include <dds/util/fs.hpp>

#include <optional>
#include <vector>

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
    source_kind kind;

    static std::optional<source_file> from_path(path_ref) noexcept;
    static std::vector<source_file>   collect_for_dir(path_ref);
};

using source_list = std::vector<source_file>;

}  // namespace dds