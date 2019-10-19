#pragma once

#include <dds/util/fs.hpp>

namespace dds {

struct sdist_params {
    fs::path project_dir;
    fs::path dest_path;
    bool     force         = false;
    bool     include_apps  = false;
    bool     include_tests = false;
};

void create_sdist(const sdist_params&);
void create_sdist_in_dir(path_ref, const sdist_params&);

}  // namespace dds