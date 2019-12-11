#pragma once

#include <dds/util/fs.hpp>

#include <pubgrub/interval.hpp>
#include <semver/range.hpp>
#include <semver/version.hpp>

#include <string_view>

namespace dds {

using version_range_set = pubgrub::interval_set<semver::version>;

struct dependency {
    std::string       name;
    version_range_set versions;

    static dependency parse_depends_string(std::string_view str);
};

/**
 * Represents a dependency listing file, which is a subset of a package manifest
 */
struct dependency_manifest {
    std::vector<dependency> dependencies;

    static dependency_manifest from_file(path_ref where);
};

}  // namespace dds
