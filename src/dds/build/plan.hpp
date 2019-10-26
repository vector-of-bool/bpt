#pragma once

#include <dds/build/compile.hpp>
#include <dds/build/params.hpp>
#include <dds/library.hpp>

#include <dds/toolchain.hpp>
#include <dds/util/fs.hpp>

namespace dds {

struct create_archive_plan {
    std::string name;
    fs::path    out_dir;

    fs::path archive_file_path(const toolchain& tc) const noexcept;

    void
    archive(const toolchain& tc, path_ref out_prefix, const std::vector<fs::path>& objects) const;
};

struct create_exe_plan {
    std::vector<fs::path> in_sources;
    std::string           name;
    fs::path              out_dir;
};

struct library_plan {
    fs::path                           out_subdir;
    std::vector<compile_file_plan>     compile_files;
    std::optional<create_archive_plan> create_archive;
    std::vector<create_exe_plan>       link_executables;

    static library_plan create(const library& lib, const library_build_params& params);
};

struct build_plan {
    std::vector<library_plan> create_libraries;

    // static build_plan generate(const build_params& params);
    void add_library(const library& lib, const library_build_params& params) {
        create_libraries.push_back(library_plan::create(lib, params));
    }

    void compile_all(const toolchain& tc, int njobs, path_ref out_prefix) const;
    void archive_all(const toolchain& tc, int njobs, path_ref out_prefix) const;
};

}  // namespace dds
