#pragma once

#include <dds/error/result_fwd.hpp>
#include <dds/util/db/db.hpp>
#include <dds/util/fs.hpp>

#include <functional>
#include <vector>

namespace dds {

class file_collector {
    std::reference_wrapper<unique_database> _db;

    explicit file_collector(unique_database& db) noexcept
        : _db(db) {}

public:
    // Create a new collector with the given database as the cache source
    [[nodiscard]] static result<file_collector> create(unique_database& db) noexcept;

    // Obtain a recursive listing of every descendant of the given directory
    result<std::vector<fs::path>> collect(path_ref) noexcept;
    // Remove the given directory from the database cache
    void forget(path_ref) noexcept;
    /// Determine whether the collector has a cache entry for the given directory
    [[nodiscard]] bool has_cached(path_ref) noexcept;
};

}  // namespace dds
