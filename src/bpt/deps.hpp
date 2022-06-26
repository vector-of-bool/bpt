#pragma once

#include <bpt/error/nonesuch.hpp>
#include <bpt/project/dependency.hpp>
#include <bpt/util/fs/path.hpp>

namespace bpt {

struct e_parse_dependency_manifest_path {
    fs::path value;
};

struct e_bad_deps_json_key : e_nonesuch {
    using e_nonesuch::e_nonesuch;
};

/**
 * Represents a dependency listing file, which is a subset of a package manifest
 */
struct dependency_manifest {
    std::vector<project_dependency> dependencies;

    static dependency_manifest from_file(path_ref where);
};

}  // namespace bpt
