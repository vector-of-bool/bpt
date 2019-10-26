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

    fs::path archive_file_path(const build_env& env) const noexcept;

    void archive(const build_env& env, const std::vector<fs::path>& objects) const;
};

struct create_exe_plan {
    std::vector<fs::path> in_sources;
    std::string           name;
    fs::path              out_dir;
};

struct library_plan {
    std::string                        name;
    fs::path                           source_root;
    fs::path                           out_subdir;
    std::vector<compile_file_plan>     compile_files;
    std::optional<create_archive_plan> create_archive;
    std::vector<create_exe_plan>       link_executables;

    static library_plan create(const library& lib, const library_build_params& params);
};

struct package_plan {
    std::string               name;
    std::string               namespace_;
    std::vector<std::string>  pkg_requires;
    std::vector<library_plan> create_libraries;

    void add_library(const library& lib, const library_build_params& params) {
        create_libraries.push_back(library_plan::create(lib, params));
    }
};

struct build_plan {
    std::vector<package_plan> build_packages;

    void compile_all(const build_env& env, int njobs) const;
    void archive_all(const build_env& env, int njobs) const;
};

}  // namespace dds
