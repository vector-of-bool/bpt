#pragma once

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
    package_manifest         manifest;
    fs::path                 path;

    sdist(package_manifest man, path_ref path_)
        : manifest(std::move(man))
        , path(path_) {}

    static sdist from_directory(path_ref p);

    std::string ident() const noexcept {
        return manifest.name + "_" + manifest.version.to_string();
    }
};

sdist create_sdist(const sdist_params&);
sdist create_sdist_in_dir(path_ref, const sdist_params&);

}  // namespace dds