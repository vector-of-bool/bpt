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

enum class dep_for_kind {
    lib,
    test,
    app,
};

struct dependency {
    dds::name         name;
    version_range_set versions;
    dep_for_kind      for_kind = dep_for_kind::lib;

    // The explicit "use" for this dependency
    std::optional<std::vector<dds::name>> uses{};

    static dependency parse_depends_string(std::string_view str);

    std::string decl_to_string() const noexcept;
};

/**
 * Represents a dependency listing file, which is a subset of a package manifest
 */
struct dependency_manifest {
    std::vector<dependency> dependencies;

    static dependency_manifest from_file(path_ref where);
};

}  // namespace dds
