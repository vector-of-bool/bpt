#pragma once

#include <dds/build/compile.hpp>
#include <dds/build/source_dir.hpp>
#include <dds/util/fs.hpp>

#include <vector>

namespace dds {

struct sroot_build_params {
    std::string               main_name;
    fs::path                  out_subdir;
    bool                      build_tests = false;
    bool                      build_apps  = false;
    std::vector<fs::path>     rt_link_libraries;
    shared_compile_file_rules compile_rules;
};

}  // namespace dds