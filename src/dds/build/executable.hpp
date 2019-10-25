#pragma once

#include <dds/util/fs.hpp>

#include <vector>

namespace dds {

class link_executable_rules {
    std::vector<fs::path> inputs;
    fs::path              output;
};

}  // namespace dds