#pragma once

#include <dds/toolchain/toolchain.hpp>
#include <dds/util/fs.hpp>

#include <vector>

namespace dds {

struct archive_rules {
    std::vector<fs::path> objects;
    fs::path              out;

    void create_archive(const toolchain& tc) const;
};

}  // namespace dds