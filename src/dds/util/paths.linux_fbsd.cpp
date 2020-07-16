#if __linux__ || __FreeBSD__

#include "./paths.hpp"

#include <dds/util/log.hpp>

#include <cstdlib>

using namespace dds;

fs::path dds::user_home_dir() {
    static auto ret = []() -> fs::path {
        auto home_env = std::getenv("HOME");
        if (!home_env) {
            log::error("No HOME environment variable set!");
            return "/";
        }
        return fs::absolute(fs::path(home_env));
    }();
    return ret;
}

fs::path dds::user_data_dir() {
    static auto ret = []() -> fs::path {
        auto xdg_data_home = std::getenv("XDG_DATA_HOME");
        if (xdg_data_home) {
            return fs::absolute(fs::path(xdg_data_home));
        }
        return user_home_dir() / ".local/share";
    }();
    return ret;
}

fs::path dds::user_cache_dir() {
    static auto ret = []() -> fs::path {
        auto xdg_cache_home = std::getenv("XDG_CACHE_HOME");
        if (xdg_cache_home) {
            return fs::absolute(fs::path(xdg_cache_home));
        }
        return user_home_dir() / ".cache";
    }();
    return ret;
}

fs::path dds::user_config_dir() {
    static auto ret = []() -> fs::path {
        auto xdg_config_home = std::getenv("XDG_CONFIG_HOME");
        if (xdg_config_home) {
            return fs::absolute(fs::path(xdg_config_home));
        }
        return user_home_dir() / ".config";
    }();
    return ret;
}

#endif
