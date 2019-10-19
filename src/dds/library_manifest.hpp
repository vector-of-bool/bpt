#pragma once

#include <dds/util.hpp>

namespace dds {

struct library_manifest {
    std::vector<fs::path>    private_includes;
    std::vector<std::string> private_defines;
    std::vector<std::string> uses;
    std::vector<std::string> links;

    static library_manifest load_from_file(const fs::path&);
};

}  // namespace dds
