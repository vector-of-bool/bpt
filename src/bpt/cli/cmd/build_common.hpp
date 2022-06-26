#include "../options.hpp"

#include <bpt/build/builder.hpp>

#include <filesystem>
#include <functional>

namespace bpt::crs {

class cache;
struct pkg_id;
struct package_info;

}  // namespace bpt::crs

namespace bpt::cli {

bpt::builder create_project_builder(const options& opts);

int handle_build_error(std::function<int()>);

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
 * @param build_all_libs If `true`, all libraries will be marked for building, otherwise none
 * @param b
 * @return crs::package_info
 */
crs::package_info fetch_cache_load_dependency(crs::cache&                  cache,
                                              const crs::pkg_id&           pkg,
                                              bool                         build_all_libs,
                                              builder&                     b,
                                              const std::filesystem::path& subdir_base);

}  // namespace bpt::cli
