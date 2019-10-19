#pragma once

#include <dds/library.hpp>
#include <dds/util.hpp>

#include <optional>
#include <vector>

namespace dds {

class project {
    std::optional<library> _main_lib;
    std::vector<library>   _submodules;

    project(std::optional<library>&& ml, std::vector<library>&& mods)
        : _main_lib(std::move(ml))
        , _submodules(std::move(mods)) {}

public:
    static project from_directory(path_ref pr_dir);

    const library* main_library() const noexcept {
        if (_main_lib) {
            return &*_main_lib;
        }
        return nullptr;
    }
};

}  // namespace dds