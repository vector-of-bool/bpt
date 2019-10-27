#pragma once

#include <dds/build/plan/compile_file.hpp>
#include <dds/util/fs.hpp>

#include <string>

namespace dds {

class create_archive_plan {
    std::string                    _name;
    fs::path                       _subdir;
    std::vector<compile_file_plan> _compile_files;

public:
    create_archive_plan(std::string_view name, path_ref subdir, std::vector<compile_file_plan> cfs)
        : _name(name)
        , _subdir(subdir)
        , _compile_files(std::move(cfs)) {}

    fs::path calc_archive_file_path(build_env_ref env) const noexcept;
    auto&    compile_files() const noexcept { return _compile_files; }

    void archive(build_env_ref env) const;
};

}  // namespace dds
