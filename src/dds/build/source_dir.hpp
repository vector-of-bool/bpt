#pragma once

#include <dds/source.hpp>
#include <dds/util/fs.hpp>

#include <vector>

namespace dds {

struct source_directory {
    fs::path path;

    std::vector<source_file> sources() const;

    bool exists() const noexcept { return fs::exists(path); }
};

}  // namespace dds
