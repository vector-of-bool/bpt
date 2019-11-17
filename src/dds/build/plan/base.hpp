#pragma once

#include <dds/db/database.hpp>
#include <dds/toolchain/toolchain.hpp>
#include <dds/util/fs.hpp>

namespace dds {

struct build_env {
    dds::toolchain toolchain;
    fs::path       output_root;
    database&      db;
};

using build_env_ref = const build_env&;

}  // namespace dds
