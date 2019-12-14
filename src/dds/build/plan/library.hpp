#pragma once

#include <dds/build/plan/archive.hpp>
#include <dds/build/plan/exe.hpp>
#include <dds/library/library.hpp>
#include <dds/usage_reqs.hpp>
#include <dds/util/fs.hpp>

#include <libman/library.hpp>

#include <optional>
#include <string>
#include <vector>

namespace dds {

/**
 * The parameters that tweak the behavior of building a library
 */
struct library_build_params {
    /// The subdirectory of the build root in which this library should place its files.
    fs::path out_subdir;
    /// Whether tests should be compiled and linked for this library
    bool build_tests = false;
    /// Whether applications should be compiled and linked for this library
    bool build_apps = false;
    /// Whether compiler warnings should be enabled for building the source files in this library.
    bool enable_warnings = false;

    /// Directories that should be on the #include search path when compiling tests
    std::vector<fs::path> test_include_dirs;
    /// Files that should be added as inputs when linking test executables
    std::vector<fs::path> test_link_files;

    /// Libraries that are used by tests
    std::vector<lm::usage> test_uses;
};

/**
 * A `library_plan` is a composite object that keeps track of the parameters for building a library,
 * including:
 *
 * - If the library has compilable library source files, a `create_archive_plan` that details the
 *   compilation of those source files and their collection into a static library archive.
 * - The executables that need to be linked when this library is built. This includes any tests and
 *   apps that are part of this library. These can be enabled/disabled by setting the appropriate
 *   values in `library_build_params`.
 * - The libraries that this library *uses*.
 * - The libraries that this library *links*.
 *
 * While there is a public constructor, it is best to use the `create` named constructor, which will
 * initialize all of the constructor parameters correctly.
 */
class library_plan {
    /// The underlying library object
    library _lib;
    /// The `create_archive_plan` for this library, if applicable
    std::optional<create_archive_plan> _create_archive;
    /// The executables that should be linked as part of this library's build
    std::vector<link_executable_plan> _link_exes;

public:
    /**
     * Construct a new `library_plan`
     * @param lib The `library` object underlying this plan.
     * @param ar The `create_archive_plan`, or `nullopt` for this library.
     * @param exes The `link_executable_plan` objects for this library.
     */
    library_plan(library                            lib,
                 std::optional<create_archive_plan> ar,
                 std::vector<link_executable_plan>  exes)
        : _lib(std::move(lib))
        , _create_archive(std::move(ar))
        , _link_exes(std::move(exes)) {}

    /**
     * Get the underlying library object
     */
    auto& library_() const noexcept { return _lib; }
    /**
     * Get the name of the library
     */
    auto& name() const noexcept { return _lib.manifest().name; }
    /**
     * The directory that defines the source root of the library.
     */
    path_ref source_root() const noexcept { return _lib.path(); }
    /**
     * A `create_archive_plan` object, or `nullopt`, depending on if this library has compiled
     * components
     */
    auto& create_archive() const noexcept { return _create_archive; }
    /**
     * The executables that should be created by this library
     */
    auto& executables() const noexcept { return _link_exes; }
    /**
     * The library identifiers that are used by this library
     */
    auto& uses() const noexcept { return _lib.manifest().uses; }
    /**
     * The library identifiers that are linked by this library
     */
    auto& links() const noexcept { return _lib.manifest().links; }

    /**
     * Named constructor: Create a new `library_plan` automatically from some build-time parameters.
     *
     * @param lib The `library` object from which we will inherit several properties.
     * @param params Parameters controlling the build of the library. i.e. if we create tests,
     * enable warnings, etc.
     *
     * The `lib` parameter defines the usage requirements of this library, and they are looked up in
     * the `ureqs` map. If there are any missing requirements, an exception will be thrown.
     */
    static library_plan create(const library& lib, const library_build_params& params);
};

}  // namespace dds