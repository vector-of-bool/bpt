#pragma once

#include <dds/catalog/get.hpp>
#include <dds/util/fs.hpp>
#include <dds/util/fs_transform.hpp>

#include <libman/package.hpp>

#include <optional>
#include <string>

namespace dds {

struct git_remote_listing {
    std::string              url;
    std::string              ref;
    std::optional<lm::usage> auto_lib;

    std::vector<fs_transformation> transforms;

    void clone(path_ref path) const;
};

}  // namespace dds