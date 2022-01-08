#pragma once

#include <dds/project/dependency.hpp>
#include <dds/util/fs/path.hpp>

namespace dds {

/**
 * Represents a dependency listing file, which is a subset of a package manifest
 */
struct dependency_manifest {
    std::vector<project_dependency> dependencies;

    static dependency_manifest from_file(path_ref where);
};

}  // namespace dds
