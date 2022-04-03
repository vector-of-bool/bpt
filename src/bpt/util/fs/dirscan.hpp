#pragma once

#include <bpt/error/result_fwd.hpp>
#include <bpt/util/db/db.hpp>
#include <bpt/util/fs/path.hpp>

#include <neo/any_range.hpp>

#include <functional>
#include <vector>

namespace bpt {

class file_collector {
    std::reference_wrapper<unique_database> _db;

    explicit file_collector(unique_database& db) noexcept
        : _db(db) {}

public:
    // Create a new collector with the given database as the cache source
    [[nodiscard]] static file_collector create(unique_database& db);

    // Obtain a recursive listing of every descendant of the given directory
    neo::any_input_range<fs::path> collect(path_ref);
    // Remove the given directory from the database cache
    void forget(path_ref) noexcept;
    /// Determine whether the collector has a cache entry for the given directory
    [[nodiscard]] bool has_cached(path_ref) noexcept;
};

}  // namespace bpt
