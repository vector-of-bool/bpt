#include "./marker.hpp"

#include <dds/util/env.hpp>
#include <dds/util/fs/io.hpp>
#include <dds/util/log.hpp>

void dds::write_error_marker(std::string_view error) noexcept {
    dds_log(trace, "[error marker {}]", error);
    auto efile_path = dds::getenv("DDS_WRITE_ERROR_MARKER");
    if (efile_path) {
        dds_log(trace, "[error marker written to [{}]]", *efile_path);
        dds::write_file(*efile_path, error);
    }
}
