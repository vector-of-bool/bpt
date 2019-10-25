#pragma once

#include <dds/build/compile.hpp>
#include <dds/build/params.hpp>
#include <dds/build/sroot.hpp>

#include <dds/toolchain.hpp>
#include <dds/util/fs.hpp>

namespace dds {

struct create_archive_plan {
    std::vector<fs::path> in_sources;
    std::string           name;
    fs::path              out_dir;
};

struct create_exe_plan {
    std::vector<fs::path> in_sources;
    std::string           name;
    fs::path              out_dir;
};

struct library_plan {
    std::vector<compile_file_plan>   compile_files;
    std::vector<create_archive_plan> create_archives;
    std::vector<create_exe_plan>     link_executables;
    fs::path                         out_subdir;

    static library_plan create(const sroot& root, const sroot_build_params& params);
};

struct build_plan {
    std::vector<library_plan> create_libraries;

    // static build_plan generate(const build_params& params);
    void add_sroot(const sroot& root, const sroot_build_params& params);

    void compile_all(const toolchain& tc, int njobs, path_ref out_prefix) const;
};

}  // namespace dds
