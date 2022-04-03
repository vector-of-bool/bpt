#pragma once

#include <bpt/error/nonesuch.hpp>
#include <json5/data.hpp>

#include <filesystem>

namespace dds {

struct e_parse_project_manifest_path {
    std::filesystem::path value;
};

struct e_parse_project_manifest_data {
    json5::data value;
};

struct e_open_project {
    std::filesystem::path value;
};

struct e_bad_pkg_yaml_key : e_nonesuch {
    using e_nonesuch::e_nonesuch;
};

}  // namespace dds