#pragma once

#include <dds/error/result_fwd.hpp>

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

struct e_write_file_path {
    fs::path value;
};
[[nodiscard]] result<void> write_file(const fs::path& path, std::string_view content) noexcept;

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

struct e_copy_file {
    fs::path source;
    fs::path dest;
};

struct e_remove_file {
    fs::path value;
};

struct e_symlink {
    fs::path symlink;
    fs::path target;
};

[[nodiscard]] result<void>
                           copy_file(path_ref source, path_ref dest, fs::copy_options opts = {}) noexcept;
[[nodiscard]] result<void> remove_file(path_ref file) noexcept;

[[nodiscard]] result<void> create_symlink(path_ref target, path_ref symlink) noexcept;

}  // namespace file_utils

}  // namespace dds