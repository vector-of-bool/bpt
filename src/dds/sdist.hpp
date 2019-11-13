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

    std::string ident() const noexcept {
        return manifest.name + "_" + manifest.version.to_string();
    }
};

inline constexpr struct sdist_compare_t {
    bool operator()(const sdist& lhs, const sdist& rhs) const {
        return std::tie(lhs.manifest.name, lhs.manifest.version)
            < std::tie(rhs.manifest.name, rhs.manifest.version);
    }
    template <typename Name, typename Version>
    bool operator()(const sdist& lhs, const std::tuple<Name, Version>& rhs) const {
        return std::tie(lhs.manifest.name, lhs.manifest.version) < rhs;
    }
    template <typename Name, typename Version>
    bool operator()(const std::tuple<Name, Version>& lhs, const sdist& rhs) const {
        return lhs < std::tie(rhs.manifest.name, rhs.manifest.version);
    }
    using is_transparent = int;
} sdist_compare;

sdist create_sdist(const sdist_params&);
sdist create_sdist_in_dir(path_ref, const sdist_params&);

}  // namespace dds