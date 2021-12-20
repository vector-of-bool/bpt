#pragma once

#include "./cache_db.hpp"

#include <dds/util/fs/path.hpp>

namespace dds::crs {

class cache_db;
struct pkg_id;

class cache {
    struct impl;
    std::shared_ptr<impl> _impl;

    explicit cache(path_ref p);

public:
    static cache open(path_ref dirpath);
    static cache open_default();

    static fs::path default_path() noexcept;

    fs::path  pkgs_dir_path() const noexcept;
    cache_db& metadata_db() noexcept;

    fs::path prefetch(const pkg_id&);
};

}  // namespace dds::crs
