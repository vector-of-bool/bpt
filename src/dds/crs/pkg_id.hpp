#pragma once

#include <dds/pkg/name.hpp>
#include <semver/version.hpp>

namespace dds::crs {

struct e_invalid_pkg_id_str {
    std::string value;
};

struct pkg_id {
    dds::name       name;
    semver::version version;
    int             meta_version;

    static pkg_id parse_str(std::string_view sv);

    std::string to_string() const noexcept;
};

struct e_no_such_pkg {
    pkg_id value;
};

}  // namespace dds::crs
