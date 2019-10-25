#include "./sroot.hpp"

using namespace dds;

shared_compile_file_rules sroot::base_compile_rules() const noexcept {
    auto                      inc_dir = include_dir();
    auto                      src_dir = this->src_dir();
    shared_compile_file_rules ret;
    if (inc_dir.exists()) {
        ret.include_dirs().push_back(inc_dir.path);
    }
    if (src_dir.exists()) {
        ret.include_dirs().push_back(src_dir.path);
    }
    return ret;
}

fs::path sroot::public_include_dir() const noexcept {
    auto inc_dir = include_dir();
    if (inc_dir.exists()) {
        return inc_dir.path;
    }
    return src_dir().path;
}