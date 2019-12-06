#pragma once

#include <dds/util/fs.hpp>

#include <libman/package.hpp>

#include <optional>
#include <string>

namespace dds {

struct git_remote_listing {
    std::string              url;
    std::string              ref;
    std::optional<lm::usage> auto_lib;

    void clone(path_ref path) const;
};

}  // namespace dds
