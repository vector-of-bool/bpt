#pragma once

#include <dds/deps.hpp>
#include <dds/pkg/id.hpp>

#include <functional>

namespace dds {

using pkg_id_provider_fn = std::function<std::vector<pkg_id>(std::string_view)>;
using deps_provider_fn   = std::function<std::vector<dependency>(const pkg_id& pk)>;

std::vector<pkg_id>
solve(const std::vector<dependency>& deps, pkg_id_provider_fn, deps_provider_fn);

}  // namespace dds
