#pragma once

#include <tuple>

#include <dds/crs/meta.hpp>
#include <dds/sdist/package.hpp>
#include <dds/temp.hpp>
#include <dds/util/fs/path.hpp>

namespace dds {

struct sdist_params {
    fs::path project_dir;
    fs::path dest_path;
    bool     force = false;
};

struct sdist {
    crs::package_meta pkg;
    fs::path          path;

    sdist(crs::package_meta pkg_, path_ref path_) noexcept
        : pkg{pkg_}
        , path{path_} {}

    static sdist from_directory(path_ref p);

    pkg_id id() const noexcept { return pkg_id{pkg.name, pkg.version}; }
};

inline constexpr struct sdist_compare_t {
    bool operator()(const sdist& lhs, const sdist& rhs) const { return lhs.id() < rhs.id(); }
    bool operator()(const sdist& lhs, const pkg_id& rhs) const { return lhs.id() < rhs; }
    bool operator()(const pkg_id& lhs, const sdist& rhs) const { return lhs < rhs.id(); }
    using is_transparent = int;
} sdist_compare;

sdist create_sdist(const sdist_params&);
sdist create_sdist_in_dir(path_ref, const sdist_params&);
void  create_sdist_targz(path_ref, const sdist_params&);

}  // namespace dds
