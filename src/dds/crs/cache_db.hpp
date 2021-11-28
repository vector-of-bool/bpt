#pragma once

#include "./meta.hpp"

#include <dds/error/result_fwd.hpp>
#include <dds/util/db/db.hpp>

#include <neo/url/url.hpp>

#include <filesystem>
#include <functional>
#include <vector>

namespace dds::crs {

/**
 * @brief An interface to a cache of CRS package metadata, either local, remote, or both.
 *
 * The CRS cache records the totality of all known packages that are available to be imported
 * and included in a potential build. Every package is either *local*, *remote*, or *both*. A
 * "local" package entry exists on the local filesystem and can be directly compiled and linked into
 * a project. A remote-only package is accessible from a remote server and can be downloaded and
 * made into a locally-available package.
 *
 * When resolving a dependency tree, a single cache DB should be consulted to understand what
 * packages are available to generate a dependency solution, regardless of whether they are (yet)
 * locally available packages.
 *
 * When downloading the metadata from a remote repository, the contents of that remote repository
 * are available as a set of CRS packages that can be imported into the local repository.
 */
class cache_db {
    std::reference_wrapper<dds::unique_database> _db;

    explicit cache_db(dds::unique_database& db) noexcept
        : _db(db) {}

    // Convenience method to return a prepared statement
    neo::sqlite3::statement& _prepare(neo::sqlite3::sql_string_literal) const;

public:
    /**
     * @brief A cache entry for a CRS package.
     *
     * Has a set of package metadata, and optionally a local path (where the package contents can be
     * found), or a URL (from whence the package contents can be obtained).
     */
    struct package_entry {
        /// The database row ID of this package
        std::int64_t package_id;
        /// The row ID of the remote that contains this package
        std::int64_t remote_id;
        /// The metadata for this cache entry
        package_meta pkg;
    };

    /**
     * @brief A database entry for a CRS remote repository
     */
    struct remote_entry {
        /// The database row ID of this package
        std::int64_t remote_id;
        /// The URL of this remote
        neo::url url;
        /// A globally unique name of this repository
        std::string unique_name;
    };

    /**
     * @brief Open a cache database for the given SQLite database.
     */
    [[nodiscard]] static cache_db open(unique_database& db);

    [[nodiscard]] std::optional<remote_entry> get_remote(neo::url_view const& url) const;

    /**
     * @brief Determine whether the given directory has a cache entry.
     */
    [[nodiscard]] bool is_cached(std::filesystem::path const& dirpath);

    /**
     * @brief Create a cache entry for the given directory if it has not yet been cached.
     */
    void ensure_cached(std::filesystem::path const& dirpath);

    /**
     * @brief Create a cache entry for the given directory.
     *
     * If the directory has already been cached, the cache entry will be renewed.
     */
    void discover_in(std::filesystem::path const&);

    /**
     * @brief Obtain a cache entry for the given CRS directory.
     *
     * If the directory has not yet been cached, it will be cached and then returned.
     */
    package_entry for_directory(std::filesystem::path const&);

    /**
     * @brief Forget metadata for the given directory
     */
    void forget_directory(std::filesystem::path const&);

    /**
     * @brief Forget every cached entry in the database
     */
    void forget_all();

    /**
     * @brief Obtain a list of package entries for the given name and version
     *
     * @param name The name of a package
     * @param version The version of the package
     */
    [[nodiscard]] std::vector<package_entry> for_package(dds::name const&       name,
                                                         semver::version const& version) const;

    /**
     * @brief Obtain a list of package entries for the given name.
     *
     * All available versions will be returned.
     *
     * @param name The name of a package.
     */
    [[nodiscard]] std::vector<package_entry> for_package(dds::name const& name) const;
};

}  // namespace dds::crs
