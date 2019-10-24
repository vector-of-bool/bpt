#pragma once

#include <semver/version.hpp>

#include <string_view>

namespace dds {

class repository;
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

void do_find_deps(const repository&, const dependency& dep, std::vector<sdist>& acc);

} // namespace detail

std::vector<sdist> find_dependencies(const repository& repo, const dependency& dep);

template <typename Iter, typename Snt>
inline std::vector<sdist> find_dependencies(const repository& repo, Iter it, Snt stop) {
    std::vector<sdist> acc;
    while (it != stop) {
        detail::do_find_deps(repo, *it++, acc);
    }
    return acc;
}

}  // namespace dds