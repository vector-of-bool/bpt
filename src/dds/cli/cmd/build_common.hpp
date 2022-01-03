#include "../options.hpp"

#include <dds/build/builder.hpp>

#include <filesystem>
#include <functional>

namespace dds::crs {

class cache;
struct pkg_id;
struct package_meta;

}  // namespace dds::crs

namespace dds::cli {

dds::builder create_project_builder(const options& opts);

int handle_build_error(std::function<int()>);

/**
 * @brief Open a CRS cache in the appropriate directory, will all requested
 * remote repositories synced and enabled.
 *
 * @param opts The options given by the user.
 */
dds::crs::cache open_ready_cache(const options& opts);

/**
 * @brief Fetch, cache, and load the given package ID.
 *
 * For a given package ID:
 *
 * - If it is not already locally cached, download the package data and store
 *   it in the local CRS cache.
 * - Load the CRS source distribution into the given builder.
 *
 * The given package must have locally cached metadata for an enabled repository.
 *
 * @param cache
 * @param pkg
 * @param b
 * @return crs::package_meta
 */
crs::package_meta fetch_cache_load_dependency(crs::cache&                  cache,
                                              const crs::pkg_id&           pkg,
                                              builder&                     b,
                                              const std::filesystem::path& subdir_base);

}  // namespace dds::cli
