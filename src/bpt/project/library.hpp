#pragma once

#include "./dependency.hpp"
#include <bpt/crs/info/library.hpp>
#include <bpt/util/name.hpp>

#include <json5/data.hpp>

#include <filesystem>
#include <optional>
#include <vector>

namespace bpt {

struct project_library {
    /// The name of the library within the project
    bpt::name name;
    /// The relative path to the library root from the root of the project
    std::filesystem::path relpath;
    /// Libraries in the same project that are used by this library
    std::vector<bpt::name> intra_using;
    std::vector<bpt::name> intra_test_using;
    /// Dependencies for this specific library
    std::vector<project_dependency> lib_dependencies;
    std::vector<project_dependency> test_dependencies;

    static project_library from_json_data(const json5::data&);
};

}  // namespace bpt
