#pragma once

#include <bpt/crs/info/package.hpp>
#include <bpt/util/fs/path.hpp>

namespace bpt {

struct sdist_params {
    fs::path project_dir;
    fs::path dest_path;
    bool     force    = false;
    int      revision = 1;
};

struct sdist {
    crs::package_info pkg;
    fs::path          path;

    sdist(crs::package_info pkg_, path_ref path_) noexcept
        : pkg{pkg_}
        , path{path_} {}

    static sdist from_directory(path_ref p);
};

sdist create_sdist(const sdist_params&);
sdist create_sdist_in_dir(path_ref, const sdist_params&);
void  create_sdist_targz(path_ref, const sdist_params&);

}  // namespace bpt
