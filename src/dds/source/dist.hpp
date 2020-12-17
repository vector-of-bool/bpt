#pragma once

#include <tuple>

#include <dds/package/manifest.hpp>
#include <dds/temp.hpp>
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

struct temporary_sdist {
    temporary_dir tmpdir;
    struct sdist  sdist;
};

inline constexpr struct sdist_compare_t {
    bool operator()(const sdist& lhs, const sdist& rhs) const {
        return lhs.manifest.id < rhs.manifest.id;
    }
    bool operator()(const sdist& lhs, const pkg_id& rhs) const { return lhs.manifest.id < rhs; }
    bool operator()(const pkg_id& lhs, const sdist& rhs) const { return lhs < rhs.manifest.id; }
    using is_transparent = int;
} sdist_compare;

sdist create_sdist(const sdist_params&);
sdist create_sdist_in_dir(path_ref, const sdist_params&);
void  create_sdist_targz(path_ref, const sdist_params&);

temporary_sdist expand_sdist_targz(path_ref targz);
temporary_sdist expand_sdist_from_istream(std::istream&, std::string_view input_name);
temporary_sdist download_expand_sdist_targz(std::string_view);

}  // namespace dds
