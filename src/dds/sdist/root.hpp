#pragma once

#include <dds/sdist/file.hpp>
#include <dds/util/fs/path.hpp>

#include <neo/any_range.hpp>

#include <ranges>
#include <vector>

namespace dds {

struct collected_sources : neo::any_range<source_file, std::input_iterator_tag>,
                           std::ranges::view_interface<collected_sources> {
    using any_range::any_range;
};

collected_sources collect_sources(path_ref dirpath);

/**
 * A `source_root` is a simple wrapper type that provides type safety and utilities to
 * represent a source root.
 */
struct source_root {
    /// The actual path to the directory
    fs::path path;

    /**
     * Generate a vector of every source file contained in this directory (including subdirectories)
     */
    std::vector<source_file> collect_sources() const;

    /**
     * Check if the directory exists
     */
    bool exists() const noexcept { return fs::exists(path); }
};

}  // namespace dds
