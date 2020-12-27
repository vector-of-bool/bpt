#pragma once

#include <dds/sdist/dist.hpp>
#include <dds/toolchain/toolchain.hpp>
#include <dds/util/fs.hpp>

#include <optional>

namespace dds {

struct build_params {
    fs::path                out_root;
    std::optional<fs::path> existing_lm_index;
    std::optional<fs::path> emit_lmi;
    dds::toolchain          toolchain;
    bool                    generate_compdb = true;
    int                     parallel_jobs   = 0;
};

}  // namespace dds