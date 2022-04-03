#ifdef __APPLE__

#include "./paths.hpp"

#include <bpt/util/env.hpp>
#include <bpt/util/log.hpp>

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

fs::path dds::user_data_dir() {
    static auto ret = []() -> fs::path {
        return fs::absolute(dds::getenv("XDG_DATA_HOME", [] {
            return user_home_dir() / "Library/Application Support";
        }));
    }();
    return ret;
}

fs::path dds::user_cache_dir() {
    static auto ret = []() -> fs::path {
        return fs::absolute(
            dds::getenv("XDG_CACHE_HOME", [] { return user_home_dir() / "Library/Caches"; }));
    }();
    return ret;
}

fs::path dds::user_config_dir() {
    static auto ret = []() -> fs::path {
        return fs::absolute(
            dds::getenv("XDG_CONFIG_HOME", [] { return user_home_dir() / "Library/Preferences"; }));
    }();
    return ret;
}

#endif
