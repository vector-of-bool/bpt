#pragma once

#include <dds/util/fs.hpp>

#include <libman/library.hpp>

#include <vector>

namespace dds {

/**
 * Represents the contents of a `library.dds`. This is somewhat a stripped-down
 * version of lm::library, to only represent exactly the parts that we want to
 * offer via `library.dds`.
 */
struct library_manifest {
    /// The name of the library
    std::string name;
    /// The libraries that the owning library "uses"
    std::vector<lm::usage> uses;
    /// The libraries that the owning library must be linked with
    std::vector<lm::usage> links;

    /**
     * Load the library manifest from an existing file
     */
    static library_manifest load_from_file(const fs::path&);
};

}  // namespace dds
