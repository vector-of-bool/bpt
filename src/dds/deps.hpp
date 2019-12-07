#pragma once

#include <dds/build/plan/full.hpp>

#include <pubgrub/interval.hpp>
#include <semver/range.hpp>
#include <semver/version.hpp>

#include <string_view>

namespace dds {

struct sdist;
class repository;

enum class version_strength {
    exact,
    patch,
    minor,
    major,
};

using version_range_set = pubgrub::interval_set<semver::version>;

struct dependency {
    std::string       name;
    version_range_set versions;

    static dependency parse_depends_string(std::string_view str);
};

build_plan create_deps_build_plan(const std::vector<sdist>& deps, build_env_ref env);

void write_libman_index(path_ref where, const build_plan& plan, const build_env& env);

}  // namespace dds
