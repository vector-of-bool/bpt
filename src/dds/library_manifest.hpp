#pragma once

#include <dds/util/fs.hpp>

#include <libman/library.hpp>

#include <vector>

namespace dds {

struct library_manifest {
    std::string            name;
    std::vector<lm::usage> uses;
    std::vector<lm::usage> links;

    static library_manifest load_from_file(const fs::path&);
};

}  // namespace dds
