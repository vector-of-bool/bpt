#pragma once

#include "./meta/package.hpp"

#include <neo/url/url.hpp>

#include <filesystem>

namespace dds::crs {

struct e_repo_open_path {
    std::filesystem::path value;
};

struct e_repo_importing_dir {
    std::filesystem::path value;
};

struct e_repo_importing_package {
    package_meta value;
};

struct e_repo_already_init {};

struct e_repo_import_pkg_already_present {};

struct e_repo_import_invalid_meta_version {
    std::string value;
};

struct e_sync_remote {
    neo::url value;
};

}  // namespace dds::crs