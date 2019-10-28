#pragma once

#include <dds/build/params.hpp>
#include <dds/package_manifest.hpp>

namespace dds {

void build(const build_params&, const package_manifest& man);

}  // namespace dds
