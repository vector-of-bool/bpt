#pragma once

#include <dds/util/fs.hpp>

#include <libman/library.hpp>

#include <vector>

namespace dds {

/**
 * Represents the contents of a `library.json5`. This is somewhat a stripped-down
 * version of lm::library, to only represent exactly the parts that we want to
 * offer via `library.json5`.
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
    static library_manifest load_from_file(path_ref);

    /**
     * Find a library manifest within a directory. This will search for a few
     * file candidates and return the result from the first matching. If none
     * match, it will return nullopt.
     */
    static std::optional<fs::path>         find_in_directory(path_ref);
    static std::optional<library_manifest> load_from_directory(path_ref);
};

}  // namespace dds
