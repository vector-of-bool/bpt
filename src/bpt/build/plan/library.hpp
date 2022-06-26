#pragma once

#include <bpt/build/plan/archive.hpp>
#include <bpt/build/plan/exe.hpp>
#include <bpt/usage_reqs.hpp>
#include <bpt/util/fs/path.hpp>

#include <bpt/crs/info/package.hpp>

#include <libman/library.hpp>

#include <optional>
#include <string>
#include <vector>

namespace bpt {

/**
 * The parameters that tweak the behavior of building a library
 */
struct library_build_params {
    /// The root directory of the library
    fs::path root;
    /// The subdirectory of the build root in which this library should place its files.
    fs::path out_subdir;
    /// Whether tests should be compiled and linked for this library
    bool build_tests = false;
    /// Whether applications should be compiled and linked for this library
    bool build_apps = false;
    /// Whether compiler warnings should be enabled for building the source files in this library.
    bool enable_warnings = false;
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
    std::string _name;
    fs::path    _lib_root;
    /// The qualified name of the library
    std::string _qual_name;
    /// The library's subdirectory within the output directory
    fs::path _subdir;
    /// The `create_archive_plan` for this library, if applicable
    std::optional<create_archive_plan> _create_archive;
    /// The executables that should be linked as part of this library's build
    std::vector<link_executable_plan> _link_exes;
    /// The headers that must be checked for independence
    std::vector<compile_file_plan> _headers;
    /// Libraries used by this library
    std::vector<lm::usage> _lib_uses;

public:
    /**
     * Construct a new `library_plan`
     * @param lib The `library_root` object underlying this plan.
     * @param ar The `create_archive_plan`, or `nullopt` for this library.
     * @param exes The `link_executable_plan` objects for this library.
     */
    library_plan(std::string                        name,
                 path_ref                           lib_root,
                 std::string_view                   qual_name,
                 fs::path                           subdir,
                 std::optional<create_archive_plan> ar,
                 std::vector<link_executable_plan>  exes,
                 std::vector<compile_file_plan>     headers,
                 std::vector<lm::usage>             lib_uses)
        : _name(name)
        , _lib_root(lib_root)
        , _qual_name(qual_name)
        , _subdir(std::move(subdir))
        , _create_archive(std::move(ar))
        , _link_exes(std::move(exes))
        , _headers(std::move(headers))
        , _lib_uses(std::move(lib_uses)) {}
    std::string_view name() const noexcept { return _name; }
    /**
     * Get the qualified name of the library, as if for a libman usage requirement
     */
    auto& qualified_name() const noexcept { return _qual_name; }
    /**
     * The output subdirectory of this library plan
     */
    path_ref output_subdirectory() const noexcept { return _subdir; }
    /**
     * The directory that defines the source root of the library.
     */
    path_ref source_root() const noexcept { return _lib_root; }
    /**
     * A `create_archive_plan` object, or `nullopt`, depending on if this library has compiled
     * components
     */
    auto& archive_plan() const noexcept { return _create_archive; }
    /**
     * The executables that should be created by this library
     */
    auto& executables() const noexcept { return _link_exes; }
    /**
     * The headers that should be checked for independence by this library
     */
    auto& headers() const noexcept { return _headers; }
    /**
     * The library identifiers that are used by this library
     */
    auto& lib_uses() const noexcept { return _lib_uses; }
    /**
     * @brief The public header source root directory for this library
     */
    fs::path public_include_dir() const noexcept;

    /**
     * Named constructor: Create a new `library_plan` automatically from some build-time parameters.
     *
     * @param lib The `library_root` from which we will inherit several properties and the build
     * will be inferred
     * @param params Parameters controlling the build of the library. i.e. if we create tests,
     * enable warnings, etc.
     * @param qual_name Optionally, provide the fully-qualified name of the library that is being
     * built
     *
     * The `lib` parameter defines the usage requirements of this library, and they are looked up in
     * the `ureqs` map. If there are any missing requirements, an exception will be thrown.
     */
    static library_plan create(path_ref                    pkg_base,
                               const crs::package_info&    pkg,
                               const crs::library_info&    lib,
                               const library_build_params& params);
};

}  // namespace bpt
