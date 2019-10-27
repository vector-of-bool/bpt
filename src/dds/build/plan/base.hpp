#pragma once

#include <dds/toolchain.hpp>
#include <dds/util/fs.hpp>

namespace dds {

struct build_env {
    dds::toolchain toolchain;
    fs::path       output_root;
};

using build_env_ref = const build_env&;

}  // namespace dds
