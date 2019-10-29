#ifdef __APPLE__

#include "./paths.hpp"

#include <spdlog/spdlog.h>

#include <cstdlib>

using namespace dds;

fs::path dds::user_home_dir() {
    static auto ret = []() -> fs::path {
        auto home_env = std::getenv("HOME");
        if (!home_env) {
            spdlog::warn("No HOME environment variable set!");
            return "/";
        }
        return fs::absolute(fs::path(home_env));
    }();
    return ret;
}

fs::path dds::user_data_dir() { return user_home_dir() / "Library/Application Support"; }
fs::path dds::user_cache_dir() { return user_home_dir() / "Library/Caches"; }
fs::path dds::user_config_dir() { return user_home_dir() / "Preferences"; }

#endif