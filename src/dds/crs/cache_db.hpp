#pragma once

#include "./meta/package.hpp"
#include "./meta/pkg_id.hpp"

#include <dds/util/db/db.hpp>

#include <neo/any_range_fwd.hpp>
#include <neo/ref_member.hpp>
#include <neo/sqlite3/fwd.hpp>
#include <neo/url/url.hpp>

namespace dds::crs {

struct e_no_such_remote_url {
    std::string value;
};

/**
 * @brief An interface to a cache of CRS package metadata.
 *
 * The CRS cache records the totality of all known packages that are available to be imported and
 * included in a potential build. Every package metadata entry corresponds to a remote package
 * repository.
 *
 * When resolving a dependency tree, a single cache DB should be consulted to understand what
 * packages are available to generate a dependency solution, regardless of whether they are (yet)
 * locally available packages.
 */
class cache_db {
    neo::ref_member<dds::unique_database> _db;

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
    [[nodiscard]] std::optional<remote_entry> get_remote_by_id(std::int64_t) const;

    void enable_remote(neo::url_view const&);
    void disable_remote(neo::url_view const&);

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
    [[nodiscard]] neo::any_input_range<package_entry>
    for_package(dds::name const& name, semver::version const& version) const;

    /**
     * @brief Obtain a list of package entries for the given name.
     *
     * All available versions will be returned.
     *
     * @param name The name of a package.
     */
    [[nodiscard]] neo::any_input_range<package_entry> for_package(dds::name const& name) const;

    /**
     * @brief Ensure that we have up-to-date package metadata from the given remote repo
     */
    void sync_remote(const neo::url_view& url) const;

    std::optional<pkg_id> lowest_version_matching(const dependency& dep) const;

    neo::sqlite3::connection_ref sqlite3_db() const noexcept;
};

}  // namespace dds::crs
