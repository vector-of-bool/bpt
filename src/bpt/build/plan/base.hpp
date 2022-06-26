#pragma once

#include <bpt/db/database.hpp>
#include <bpt/toolchain/toolchain.hpp>
#include <bpt/usage_reqs.hpp>

#include <filesystem>

namespace bpt {

struct build_env {
    bpt::toolchain        toolchain;
    std::filesystem::path output_root;
    database&             db;

    toolchain_knobs knobs;

    const usage_requirements& ureqs;
};

using build_env_ref = const build_env&;

}  // namespace bpt
