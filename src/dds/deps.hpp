#pragma once

#include <dds/build/plan/full.hpp>
#include <dds/repo/repo.hpp>

#include <semver/version.hpp>

#include <string_view>

namespace dds {

struct sdist;

enum class version_strength {
    exact,
    patch,
    minor,
    major,
};

struct dependency {
    std::string     name;
    semver::version version;

    static dependency parse_depends_string(std::string_view str);
};

namespace detail {

void do_find_deps(const std::vector<sdist>&, const dependency& dep, std::vector<sdist>& acc);
void sort_sdists(std::vector<sdist>& sds);

}  // namespace detail

std::vector<sdist> find_dependencies(const repository& repo, const dependency& dep);

template <typename Iter, typename Snt>
inline std::vector<sdist> find_dependencies(const repository& repo, Iter it, Snt stop) {
    std::vector<sdist> acc;
    auto               all_sds = repo.load_sdists();
    detail::sort_sdists(all_sds);
    while (it != stop) {
        detail::do_find_deps(all_sds, *it++, acc);
    }
    return acc;
}

build_plan create_deps_build_plan(const std::vector<sdist>& deps, build_env_ref env);

void write_libman_index(path_ref where, const build_plan& plan, const build_env& env);

}  // namespace dds