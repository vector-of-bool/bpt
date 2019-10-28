#pragma once

#include <dds/toolchain/toolchain.hpp>
#include <dds/util/fs.hpp>

namespace dds {

struct build_params {
    fs::path       root;
    fs::path       out_root;
    fs::path       lm_index;
    dds::toolchain toolchain;
    bool           do_export       = false;
    bool           build_tests     = false;
    bool           enable_warnings = false;
    bool           build_apps      = false;
    bool           build_deps      = false;
    bool           generate_compdb = true;
    int            parallel_jobs   = 0;
};

}  // namespace dds