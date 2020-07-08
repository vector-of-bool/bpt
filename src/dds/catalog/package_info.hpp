#pragma once

#include "./remote/git.hpp"

#include <dds/deps.hpp>
#include <dds/package/id.hpp>
#include <dds/util/fs_transform.hpp>
#include <dds/util/glob.hpp>

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace dds {

struct package_info {
    package_id              ident;
    std::vector<dependency> deps;
    std::string             description;

    std::variant<std::monostate, git_remote_listing> remote;
};

}  // namespace dds
