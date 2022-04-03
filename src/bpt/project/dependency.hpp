#pragma once

#include <bpt/crs/info/dependency.hpp>
#include <bpt/util/name.hpp>

#include <json5/data.hpp>

#include <optional>
#include <vector>

namespace bpt {

struct e_parse_dep_shorthand_string {
    std::string value;
};

struct e_parse_dep_range_shorthand_string {
    std::string value;
};

/**
 * @brief A depedency declared by a project (Not a CRS dist)
 */
struct project_dependency {
    /// The name of the dependency package
    bpt::name dep_name;
    /// A set of version ranges that are acceptable
    crs::version_range_set acceptable_versions;
    /// Libraries from the dependency that will be explicitly used
    std::optional<std::vector<bpt::name>> explicit_uses = std::nullopt;

    static project_dependency parse_dep_range_shorthand(std::string_view sv);
    static project_dependency from_shorthand_string(std::string_view sv);
    static project_dependency from_json_data(const json5::data&);

    crs::dependency as_crs_dependency() const noexcept;
};

}  // namespace bpt
