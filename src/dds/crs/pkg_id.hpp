#pragma once

#include <dds/pkg/name.hpp>
#include <semver/version.hpp>

namespace dds::crs {

struct pkg_id {
    dds::name       name;
    semver::version version;
    int             meta_version;
};

}  // namespace dds::crs
