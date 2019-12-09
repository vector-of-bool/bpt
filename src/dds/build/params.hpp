#pragma once

#include <dds/sdist.hpp>
#include <dds/toolchain/toolchain.hpp>
#include <dds/util/fs.hpp>

#include <optional>

namespace dds {

struct build_params {
    fs::path                root;
    fs::path                out_root;
    std::optional<fs::path> existing_lm_index;
    dds::toolchain          toolchain;
    std::vector<sdist>      dep_sdists;
    bool                    do_export       = false;
    bool                    build_tests     = false;
    bool                    enable_warnings = false;
    bool                    build_apps      = false;
    bool                    generate_compdb = true;
    int                     parallel_jobs   = 0;
};

}  // namespace dds