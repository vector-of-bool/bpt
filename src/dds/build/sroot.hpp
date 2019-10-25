#pragma once

#include <dds/build/compile.hpp>
#include <dds/build/source_dir.hpp>
#include <dds/util/fs.hpp>

#include <vector>

namespace dds {

struct sroot {
    fs::path path;

    source_directory src_dir() const noexcept { return source_directory{path / "src"}; };
    source_directory include_dir() const noexcept { return source_directory{path / "include"}; }

    fs::path public_include_dir() const noexcept;

    shared_compile_file_rules base_compile_rules() const noexcept;
};

struct sroot_build_params {
    std::string               main_name;
    fs::path                  out_dir;
    bool                      build_tests = false;
    bool                      build_apps  = false;
    std::vector<fs::path>     rt_link_libraries;
    shared_compile_file_rules compile_rules;
};

}  // namespace dds