#pragma once

#include <tuple>

#include <dds/package_manifest.hpp>
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
    template <typename Name, typename Version>
    bool operator()(const sdist& lhs, const std::tuple<Name, Version>& rhs) const {
        auto&& [name, ver] = rhs;
        return lhs.manifest.pk_id < package_id{name, ver};
    }
    template <typename Name, typename Version>
    bool operator()(const std::tuple<Name, Version>& lhs, const sdist& rhs) const {
        auto&& [name, ver] = lhs;
        return package_id{name, ver} < rhs.manifest.pk_id;
    }
    using is_transparent = int;
} sdist_compare;

sdist create_sdist(const sdist_params&);
sdist create_sdist_in_dir(path_ref, const sdist_params&);

}  // namespace dds