#pragma once

#include <bpt/util/name.hpp>
#include <semver/version.hpp>

namespace dds::crs {

struct e_invalid_pkg_id_str {
    std::string value;
};

struct pkg_id {
    /// The name of the package
    dds::name name;
    /// The version of the code in the package package
    semver::version version;
    /// The revision of the package itself
    int revision;

    /// Parse the given string as a package-id string
    static pkg_id parse(std::string_view sv);

    /// Convert the package ID back into a string
    std::string to_string() const noexcept;

    bool operator<(const pkg_id&) const noexcept;
    bool operator==(const pkg_id&) const noexcept = default;
};

struct e_no_such_pkg {
    pkg_id value;
};

}  // namespace dds::crs
