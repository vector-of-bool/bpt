#include "./marker.hpp"

#include <dds/util/env.hpp>
#include <dds/util/log.hpp>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <fstream>

void dds::write_error_marker(std::string_view error) noexcept {
    dds_log(trace, "[error marker {}]", error);
    auto efile_path = dds::getenv("DDS_WRITE_ERROR_MARKER");
    if (efile_path) {
        std::ofstream outfile{*efile_path, std::ios::binary};
        fmt::print(outfile, "{}", error);
    }
}
