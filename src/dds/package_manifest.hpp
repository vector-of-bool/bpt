#pragma once

#include <dds/deps.hpp>
#include <dds/util/fs.hpp>
#include <semver/version.hpp>

#include <optional>
#include <string>
#include <vector>

namespace dds {

struct package_manifest {
    std::string             name;
    std::string             namespace_;
    semver::version         version;
    std::vector<dependency> dependencies;
    static package_manifest load_from_file(path_ref);
};

}  // namespace dds