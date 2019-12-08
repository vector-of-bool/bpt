#pragma once

#include <dds/build/plan/full.hpp>

#include <pubgrub/interval.hpp>
#include <semver/range.hpp>
#include <semver/version.hpp>

#include <string_view>

namespace dds {

using version_range_set = pubgrub::interval_set<semver::version>;

struct dependency {
    std::string       name;
    version_range_set versions;

    static dependency parse_depends_string(std::string_view str);
};

}  // namespace dds
