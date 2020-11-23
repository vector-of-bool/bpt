#pragma once

#include <dds/deps.hpp>
#include <dds/package/id.hpp>
#include <dds/util/fs.hpp>

#include <optional>
#include <string>
#include <vector>

namespace dds {

/**
 * Possible values for test_driver in a package.json5
 */
enum class test_lib {
    catch_,
    catch_main,
};

/**
 * Struct representing the contents of a `packaeg.dds` file.
 */
struct package_manifest {
    /// The package ID, as determined by `Name` and `Version` together
    package_id pkg_id;
    /// The declared `Namespace` of the package. This directly corresponds with the libman Namespace
    std::string namespace_;
    /// The `test_driver` that this package declares, or `nullopt` if absent.
    std::optional<test_lib> test_driver;
    /// The dependencies declared with the `Depends` fields, if any.
    std::vector<dependency> dependencies;

    /**
     * Load a package manifest from a file on disk.
     */
    static package_manifest load_from_file(path_ref);
    /**
     * @brief Load a package manifest from an in-memory string
     */
    static package_manifest load_from_json5_str(std::string_view, std::string_view input_name);

    /**
     * Find a package manifest contained within a directory. This will search
     * for a few file candidates and return the result from the first matching.
     * If none match, it will return nullopt.
     */
    static std::optional<fs::path>         find_in_directory(path_ref);
    static std::optional<package_manifest> load_from_directory(path_ref);
};

}  // namespace dds