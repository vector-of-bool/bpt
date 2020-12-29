#ifdef __APPLE__

#include "./paths.hpp"

#include <dds/util/env.hpp>
#include <dds/util/log.hpp>

#include <cstdlib>

using namespace dds;

fs::path dds::user_home_dir() {
    static auto ret = []() -> fs::path {
        return fs::absolute(dds::getenv("HOME", [] {
            dds_log(error, "No HOME environment variable set!");
            return "/";
        }));
    }();
    return ret;
}

fs::path dds::user_data_dir() { return user_home_dir() / "Library/Application Support"; }
fs::path dds::user_cache_dir() { return user_home_dir() / "Library/Caches"; }
fs::path dds::user_config_dir() { return user_home_dir() / "Preferences"; }

#endif
