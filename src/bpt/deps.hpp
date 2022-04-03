#pragma once

#include <bpt/project/dependency.hpp>
#include <bpt/util/fs/path.hpp>

namespace bpt {

/**
 * Represents a dependency listing file, which is a subset of a package manifest
 */
struct dependency_manifest {
    std::vector<project_dependency> dependencies;

    static dependency_manifest from_file(path_ref where);
};

}  // namespace bpt
