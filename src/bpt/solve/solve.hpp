#pragma once

#include <bpt/crs/info/pkg_id.hpp>
#include <bpt/error/nonesuch.hpp>

#include <libman/library.hpp>
#include <neo/any_range.hpp>

#include <vector>

namespace bpt {

namespace crs {

class cache_db;
struct dependency;

}  // namespace crs

struct e_usage_no_such_lib {};

struct e_dependency_solve_failure {};
struct e_dependency_solve_failure_explanation {
    std::string value;
};

struct e_nonesuch_package : e_nonesuch {
    using e_nonesuch::e_nonesuch;
};

struct e_nonesuch_using_library {
    bpt::name  pkg_name;
    e_nonesuch lib;
};

std::vector<crs::pkg_id> solve(crs::cache_db const&, neo::any_input_range<crs::dependency>);

}  // namespace bpt
