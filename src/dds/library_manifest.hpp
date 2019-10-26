#pragma once

#include <dds/util/fs.hpp>

#include <vector>

namespace dds {

struct library_manifest {
    std::string              name;
    std::vector<fs::path>    private_includes;
    std::vector<std::string> private_defines;
    std::vector<std::string> uses;
    std::vector<std::string> links;

    static library_manifest load_from_file(const fs::path&);
};

}  // namespace dds
