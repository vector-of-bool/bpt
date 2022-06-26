#pragma once

#include <bpt/util/fs/path.hpp>

namespace bpt {

fs::path user_home_dir();
fs::path user_data_dir();
fs::path user_cache_dir();
fs::path user_config_dir();

inline fs::path bpt_data_dir() { return user_data_dir() / "bpt"; }
inline fs::path bpt_cache_dir() { return user_cache_dir() / "bpt"; }
inline fs::path bpt_config_dir() { return user_config_dir() / "bpt"; }

}  // namespace bpt