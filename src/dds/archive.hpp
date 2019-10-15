#pragma once

#include <dds/util.hpp>

#include <vector>

namespace dds {

struct archive_rules {
    std::vector<fs::path> objects;
    fs::path              out;
};

}  // namespace dds