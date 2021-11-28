#pragma once

#include <dds/db/database.hpp>
#include <dds/toolchain/toolchain.hpp>
#include <dds/usage_reqs.hpp>

#include <filesystem>

namespace dds {

struct build_env {
    dds::toolchain        toolchain;
    std::filesystem::path output_root;
    database&             db;

    toolchain_knobs knobs;

    const usage_requirements& ureqs;
};

using build_env_ref = const build_env&;

}  // namespace dds
