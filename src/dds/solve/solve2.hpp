#pragma once

#include <dds/crs/pkg_id.hpp>

#include <libman/library.hpp>
#include <neo/any_range.hpp>

#include <vector>

namespace dds {

namespace crs {

class cache_db;
struct dependency;

}  // namespace crs

struct e_usage_namespace_mismatch {};
struct e_usage_no_such_lib {};

struct e_dependency_solve_failure {};
struct e_dependency_solve_failure_explanation {
    std::string value;
};

std::vector<crs::pkg_id> solve2(crs::cache_db const&, neo::any_input_range<crs::dependency>);

}  // namespace dds
