#pragma once

#include "./dependency.hpp"
#include <dds/crs/info/library.hpp>
#include <dds/util/name.hpp>

#include <json5/data.hpp>

#include <filesystem>
#include <optional>
#include <vector>

namespace dds {

struct project_library {
    /// The name of the library within the project
    dds::name name;
    /// The relative path to the library root from the root of the project
    std::filesystem::path relpath;
    /// Libraries in the same project that are used by this library
    std::optional<std::vector<crs::intra_usage>> intra_uses;
    /// Dependencies for this specific library
    std::vector<project_dependency> lib_dependencies;

    static project_library from_json_data(const json5::data&);
};

}  // namespace dds
