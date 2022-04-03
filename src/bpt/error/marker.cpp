#include "./marker.hpp"

#include <bpt/util/env.hpp>
#include <bpt/util/fs/io.hpp>
#include <bpt/util/log.hpp>

void bpt::write_error_marker(std::string_view error) noexcept {
    bpt_log(trace, "[error marker {}]", error);
    auto efile_path = bpt::getenv("BPT_WRITE_ERROR_MARKER");
    if (efile_path) {
        bpt_log(trace, "[error marker written to [{}]]", *efile_path);
        bpt::write_file(*efile_path, error);
    }
}
