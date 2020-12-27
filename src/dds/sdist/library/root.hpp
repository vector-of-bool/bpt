#pragma once

#include "./manifest.hpp"

#include "../file.hpp"
#include "../root.hpp"
#include <dds/build/plan/compile_file.hpp>

#include <string>

namespace dds {

/**
 * Represents a library that exists on the filesystem
 */
class library_root {
    // The path containing the source directories for this library
    fs::path _path;
    // The sources that are part of this library
    source_list _sources;
    // The library manifest associated with this library (may be generated)
    library_manifest _man;

    // Private constructor. Use named constructor `from_directory`, which will build
    // the construct arguments approperiately
    library_root(path_ref dir, source_list&& src, library_manifest&& man)
        : _path(dir)
        , _sources(std::move(src))
        , _man(std::move(man)) {}

public:
    /**
     * Create a library object that refers to the library contained at the given
     * directory path. This will load the sources and manifest properly and
     * return the resulting library object.
     */
    static library_root from_directory(path_ref);

    /**
     * Obtain the manifest for this library
     */
    const library_manifest& manifest() const noexcept { return _man; }

    /**
     * The `src/` directory for this library.
     */
    source_root src_source_root() const noexcept { return source_root{path() / "src"}; }

    /**
     * The `include/` directory for this library
     */
    source_root include_source_root() const noexcept { return source_root{path() / "include"}; }

    /**
     * The root path for this library (parent of `src/` and `include/`, if present)
     */
    path_ref path() const noexcept { return _path; }

    /**
     * The directory that should be considered the "public" include directory.
     * Dependees that want to use this library should add this to their #include
     * search path.
     */
    fs::path public_include_dir() const noexcept;

    /**
     * The directory that contains the "private" heders for this libary. This
     * directory should be added to the search path of the library when it is
     * being built, but NOT to the search path of the dependees.
     */
    fs::path private_include_dir() const noexcept;

    /**
     * Get the sources that this library contains
     */
    const source_list& all_sources() const noexcept { return _sources; }

    /**
     * Generate a compile rules object that should be used when compiling
     * this library.
     */
    shared_compile_file_rules base_compile_rules() const noexcept;
};

/**
 * Given the root source directory of a project/package/sdist, collect all of
 * the libraries that it contains. There may be a library directly in `where`,
 * but there might also be libraries in `where/libs`. This function will find
 * them all.
 */
std::vector<library_root> collect_libraries(path_ref where);

}  // namespace dds