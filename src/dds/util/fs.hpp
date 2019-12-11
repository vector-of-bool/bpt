#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace dds {

inline namespace file_utils {

namespace fs = std::filesystem;

using path_ref = const fs::path&;

std::fstream open(const fs::path& filepath, std::ios::openmode mode, std::error_code& ec);
std::string  slurp_file(const fs::path& path, std::error_code& ec);

inline std::fstream open(const fs::path& filepath, std::ios::openmode mode) {
    std::error_code ec;
    auto            ret = dds::open(filepath, mode, ec);
    if (ec) {
        throw std::system_error{ec, "Error opening file: " + filepath.string()};
    }
    return ret;
}

inline std::string slurp_file(const fs::path& path) {
    std::error_code ec;
    auto            contents = dds::slurp_file(path, ec);
    if (ec) {
        throw std::system_error{ec, "Reading file: " + path.string()};
    }
    return contents;
}

void safe_rename(path_ref source, path_ref dest);

}  // namespace file_utils

}  // namespace dds