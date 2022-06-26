#ifdef __APPLE__

#include "./paths.hpp"

#include <bpt/util/env.hpp>
#include <bpt/util/log.hpp>

#include <cstdlib>

using namespace bpt;

fs::path bpt::user_home_dir() {
    static auto ret = []() -> fs::path {
        return fs::absolute(bpt::getenv("HOME", [] {
            bpt_log(error, "No HOME environment variable set!");
            return "/";
        }));
    }();
    return ret;
}

fs::path bpt::user_data_dir() {
    static auto ret = []() -> fs::path {
        return fs::absolute(bpt::getenv("XDG_DATA_HOME", [] {
            return user_home_dir() / "Library/Application Support";
        }));
    }();
    return ret;
}

fs::path bpt::user_cache_dir() {
    static auto ret = []() -> fs::path {
        return fs::absolute(
            bpt::getenv("XDG_CACHE_HOME", [] { return user_home_dir() / "Library/Caches"; }));
    }();
    return ret;
}

fs::path bpt::user_config_dir() {
    static auto ret = []() -> fs::path {
        return fs::absolute(
            bpt::getenv("XDG_CONFIG_HOME", [] { return user_home_dir() / "Library/Preferences"; }));
    }();
    return ret;
}

#endif
