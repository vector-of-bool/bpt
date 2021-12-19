#pragma once

#include <dds/pkg/name.hpp>

#include <libman/library.hpp>
#include <pubgrub/interval.hpp>
#include <semver/range.hpp>
#include <semver/version.hpp>

#include <string>

namespace dds::crs {

using version_range_set = pubgrub::interval_set<semver::version>;

enum class usage_kind {
    lib,
    test,
    app,
};

struct dependency {
    dds::name              name;
    version_range_set      acceptable_versions;
    usage_kind             kind;
    std::vector<lm::usage> uses;

    std::string decl_to_string() const noexcept;
};

}  // namespace dds::crs
