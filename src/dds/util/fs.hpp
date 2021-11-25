#pragma once

#include <dds/error/result_fwd.hpp>

#include "./fs/io.hpp"
#include "./fs/path.hpp"
#include "./fs/shutil.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace dds {

inline namespace file_utils {

namespace fs = std::filesystem;

std::fstream open(path_ref filepath, std::ios::openmode mode, std::error_code& ec);
std::fstream open(path_ref filepath, std::ios::openmode mode);

std::string slurp_file(path_ref path, std::error_code& ec);

std::string slurp_file(path_ref path);

}  // namespace file_utils

}  // namespace dds