#pragma once

#include <dds/build/plan/full.hpp>

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

struct dependency {
    std::string   name;
    semver::range version_range;

    static dependency parse_depends_string(std::string_view str);
};

namespace detail {

void do_find_deps(const repository&, const dependency& dep, std::vector<sdist>& acc);

}  // namespace detail

std::vector<sdist> find_dependencies(const repository& repo, const dependency& dep);

template <typename Iter, typename Snt>
inline std::vector<sdist> find_dependencies(const repository& repo, Iter it, Snt stop) {
    std::vector<sdist> acc;
    while (it != stop) {
        detail::do_find_deps(repo, *it++, acc);
    }
    return acc;
}

build_plan create_deps_build_plan(const std::vector<sdist>& deps, build_env_ref env);

void write_libman_index(path_ref where, const build_plan& plan, const build_env& env);

}  // namespace dds
