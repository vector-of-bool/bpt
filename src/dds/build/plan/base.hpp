#pragma once

#include <dds/db/database.hpp>
#include <dds/toolchain/toolchain.hpp>
#include <dds/usage_reqs.hpp>
#include <dds/util/fs.hpp>

namespace dds {

struct build_env {
    dds::toolchain toolchain;
    fs::path       output_root;
    database&      db;

    toolchain_knobs knobs;

    const usage_requirement_map& ureqs;
};

using build_env_ref = const build_env&;

}  // namespace dds
