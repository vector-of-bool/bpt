#pragma once

#include <bpt/sdist/dist.hpp>
#include <bpt/toolchain/toolchain.hpp>
#include <bpt/util/fs/path.hpp>

#include <optional>

namespace bpt {

struct build_params {
    fs::path                out_root;
    std::optional<fs::path> emit_built_json;
    std::optional<fs::path> emit_cmake{};
    std::optional<fs::path> tweaks_dir{};
    bpt::toolchain          toolchain;
    bool                    generate_compdb = true;
    int                     parallel_jobs   = 0;
};

}  // namespace bpt
