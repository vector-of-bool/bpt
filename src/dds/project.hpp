#pragma once

#include <dds/library.hpp>
#include <dds/package_manifest.hpp>
#include <dds/util/fs.hpp>

#include <optional>
#include <vector>

namespace dds {

class project {
    fs::path               _root;
    std::optional<library> _main_lib;
    std::vector<library>   _submodules;
    package_manifest       _man;

    project(path_ref                 root,
            std::optional<library>&& ml,
            std::vector<library>&&   mods,
            package_manifest&&       man)
        : _root(root)
        , _main_lib(std::move(ml))
        , _submodules(std::move(mods))
        , _man(std::move(man)) {}

public:
    static project from_directory(path_ref pr_dir);

    const library* main_library() const noexcept {
        if (_main_lib) {
            return &*_main_lib;
        }
        return nullptr;
    }

    auto& submodules() const noexcept { return _submodules; }
    auto& manifest() const noexcept { return _man; }

    path_ref root() const noexcept { return _root; }
};

}  // namespace dds