#pragma once

#include "./library.hpp"

#include <dds/crs/meta.hpp>
#include <dds/pkg/name.hpp>

#include <json5/data.hpp>
#include <neo/any_range.hpp>
#include <semver/version.hpp>

#include <optional>
#include <vector>

namespace dds {

struct project_manifest {
    dds::name       name;
    semver::version version;

    std::optional<dds::name>        namespace_;
    std::vector<project_library>    libraries;
    std::vector<project_dependency> root_dependencies;

    static project_manifest from_json_data(const json5::data& data);

    crs::package_meta as_crs_package_meta() const noexcept;
};

struct project {
    std::filesystem::path           path;
    std::optional<project_manifest> manifest;

    project_manifest get_manifest_or_default() const noexcept;

    static project open_directory(const std::filesystem::path&);
};

}  // namespace dds
