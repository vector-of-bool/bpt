#pragma once

#include <tuple>

#include <dds/package/manifest.hpp>
#include <dds/util/fs.hpp>

namespace dds {

struct sdist_params {
    fs::path project_dir;
    fs::path dest_path;
    bool     force         = false;
    bool     include_apps  = false;
    bool     include_tests = false;
};

struct sdist {
    package_manifest manifest;
    fs::path         path;

    sdist(package_manifest man, path_ref path_)
        : manifest(std::move(man))
        , path(path_) {}

    static sdist from_directory(path_ref p);
};

inline constexpr struct sdist_compare_t {
    bool operator()(const sdist& lhs, const sdist& rhs) const {
        return lhs.manifest.pk_id < rhs.manifest.pk_id;
    }
    bool operator()(const sdist& lhs, const package_id& rhs) const {
        return lhs.manifest.pk_id < rhs;
    }
    bool operator()(const package_id& lhs, const sdist& rhs) const {
        return lhs < rhs.manifest.pk_id;
    }
    using is_transparent = int;
} sdist_compare;

sdist create_sdist(const sdist_params&);
sdist create_sdist_in_dir(path_ref, const sdist_params&);

}  // namespace dds