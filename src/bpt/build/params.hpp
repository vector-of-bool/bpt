#pragma once

#include <bpt/sdist/dist.hpp>
#include <bpt/toolchain/toolchain.hpp>
#include <bpt/util/fs/path.hpp>

#include <optional>

namespace dds {

struct build_params {
    fs::path                out_root;
    std::optional<fs::path> existing_lm_index;
    std::optional<fs::path> emit_lmi;
    std::optional<fs::path> emit_cmake{};
    std::optional<fs::path> tweaks_dir{};
    dds::toolchain          toolchain;
    bool                    generate_compdb = true;
    int                     parallel_jobs   = 0;
};

}  // namespace dds
