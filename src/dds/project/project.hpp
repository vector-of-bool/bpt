#pragma once

#include "./library.hpp"

#include "./spdx.hpp"

#include <dds/crs/info/package.hpp>
#include <dds/util/name.hpp>

#include <json5/data.hpp>
#include <neo/any_range.hpp>
#include <neo/url/url.hpp>
#include <semver/version.hpp>

#include <filesystem>
#include <optional>
#include <vector>

namespace dds {

struct project_manifest {
    dds::name       name;
    semver::version version;

    std::optional<dds::name>        namespace_;
    std::vector<project_library>    libraries;
    std::vector<project_dependency> root_dependencies;

    std::optional<std::vector<std::string>>     authors;
    std::optional<std::string>                  description;
    std::optional<neo::url>                     documentation;
    std::optional<std::filesystem::path>        readme;
    std::optional<neo::url>                     homepage;
    std::optional<neo::url>                     repository;
    std::optional<sbs::spdx_license_expression> license;
    std::optional<std::filesystem::path>        license_file;

    bool is_private = false;

    static project_manifest from_json_data(const json5::data&                          data,
                                           std::optional<std::filesystem::path> const& proj_dir);

    crs::package_info as_crs_package_meta() const noexcept;
};

struct project {
    std::filesystem::path           path;
    std::optional<project_manifest> manifest;

    static project open_directory(const std::filesystem::path&);
};

}  // namespace dds
