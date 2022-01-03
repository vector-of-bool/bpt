#pragma once

#include <filesystem>
#include <memory>

namespace dds::crs {

class cache_db;
struct pkg_id;

/**
 * @brief Implements a cache of CRS source distributions along with a cache of CRS metadata from
 * some number of repositories.
 *
 * The packages available for use are controlled with a @ref cache_db that is stored
 * in the cache directory.
 */
class cache {
    struct impl;
    std::shared_ptr<impl> _impl;

    explicit cache(std::filesystem::path const& p);

public:
    /**
     * @brief Open a cache in the given directory
     *
     * @param dirpath A directory in which to create/open a CRS cache. If it does
     * not exist, a new cache will be initialized.
     */
    static cache open(std::filesystem::path const& dirpath);

    /**
     * @brief Get the default directory for the CRS cache for the current user.
     */
    static std::filesystem::path default_path() noexcept;

    /**
     * @brief The CRS metadata database for this CRS cache.
     *
     * @return cache_db&
     */
    cache_db& metadata_db() noexcept;

    /**
     * @brief Ensure that the given package has a locally cached copy of its source distribution.
     *
     * If the package has already been pre-fetched, it will not be pulled again.
     *
     * @returns The directory of the source r for the requested package.
     *
     * @throws std::exception if the given package does not have CRS metadata in any of the
     * currently enabled remotes for the metadata database, even if there is an existing local copy
     * of the package.
     */
    std::filesystem::path prefetch(const pkg_id&);
};

}  // namespace dds::crs
