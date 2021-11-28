#pragma once

#include "./fs/path.hpp"
#include <dds/error/result_fwd.hpp>

namespace dds {

struct e_compress_error {
    std::string value;
};
[[nodiscard]] result<void> compress_file_gz(fs::path in_file, fs::path out_file) noexcept;

struct e_decompress_error {
    std::string value;
};
[[nodiscard]] result<void> decompress_file_gz(fs::path in_file, fs::path out_file) noexcept;

}  // namespace dds
