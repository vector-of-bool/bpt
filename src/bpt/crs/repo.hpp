#pragma once

#include "./info/package.hpp"

#include <bpt/util/db/db.hpp>

#include <neo/any_range.hpp>

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace bpt::crs {

class repository {
    unique_database       _db;
    std::filesystem::path _dirpath;

    neo::sqlite3::statement& _prepare(neo::sqlite3::sql_string_literal) const;

    explicit repository(unique_database&& db, const std::filesystem::path& dirpath) noexcept
        : _db(std::move(db))
        , _dirpath(dirpath) {}

    void _vacuum_and_compress() const;

public:
    static repository create(const std::filesystem::path& directory, std::string_view name);
    static repository open_existing(const std::filesystem::path& directory);

    std::filesystem::path subdir_of(const package_info&) const noexcept;

    auto        pkg_dir() const noexcept { return _dirpath / "pkg"; }
    auto&       root() const noexcept { return _dirpath; }
    std::string name() const;

    void import_targz(const std::filesystem::path& tgz_path);
    void import_dir(const std::filesystem::path& dirpath);

    void remove_pkg(const package_info&);

    neo::any_input_range<package_info> all_packages() const;
};

struct ev_repo_imported_package {
    repository const&            into_repo;
    std::filesystem::path const& from_path;
    package_info const&          pkg_meta;
};

}  // namespace bpt::crs