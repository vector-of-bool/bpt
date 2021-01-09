#pragma once

#include <dds/pkg/name.hpp>
#include <dds/util/fs.hpp>

#include <pubgrub/interval.hpp>
#include <semver/range.hpp>
#include <semver/version.hpp>

#include <string_view>

namespace dds {

using version_range_set = pubgrub::interval_set<semver::version>;

struct e_dependency_string {
    std::string value;
};

struct dependency {
    dds::name         name;
    version_range_set versions;

    static dependency parse_depends_string(std::string_view str);

    std::string to_string() const noexcept;
};

/**
 * Represents a dependency listing file, which is a subset of a package manifest
 */
struct dependency_manifest {
    std::vector<dependency> dependencies;

    static dependency_manifest from_file(path_ref where);
};

}  // namespace dds
