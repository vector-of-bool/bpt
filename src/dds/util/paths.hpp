#pragma once

#include <dds/util/fs.hpp>

namespace dds {

fs::path user_home_dir();
fs::path user_data_dir();
fs::path user_cache_dir();
fs::path user_config_dir();

inline fs::path dds_data_dir() { return user_data_dir() / "dds"; }
inline fs::path dds_cache_dir() { return user_cache_dir() / "dds-cache"; }
inline fs::path dds_config_dir() { return user_config_dir() / "dds"; }

}  // namespace dds