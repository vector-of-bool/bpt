#pragma once

#include "./path.hpp"

namespace dds::inline file_utils {

struct e_open_file_path {
    fs::path value;
};

struct e_write_file_path {
    fs::path value;
};

struct e_read_file_path {
    fs::path value;
};

[[nodiscard]] result<std::fstream> open_file(path_ref filepath, std::ios::openmode) noexcept;
[[nodiscard]] result<void>         write_file(path_ref path, std::string_view content) noexcept;
[[nodiscard]] result<std::string>  read_file(path_ref path) noexcept;

}  // namespace dds::inline file_utils