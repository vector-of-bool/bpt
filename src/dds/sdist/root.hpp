#pragma once

#include <dds/sdist/file.hpp>
#include <dds/util/fs.hpp>

#include <vector>

namespace dds {

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
