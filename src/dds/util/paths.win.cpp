#ifdef _WIN32

#include "./paths.hpp"

#include <spdlog/spdlog.h>

#include <cstdlib>

using namespace dds;

fs::path dds::user_home_dir() {
    static auto ret = []() -> fs::path {
        auto userprofile_env = std::getenv("USERPROFILE");
        if (!userprofile_env) {
            spdlog::warn("No USERPROFILE environment variable set!");
            return "/";
        }
        return fs::absolute(fs::path(userprofile_env));
    }();
    return ret;
}

namespace {

fs::path appdatalocal_dir() {
    auto env = std::getenv("LocalAppData");
    assert(env);

    return fs::absolute(fs::path(env));
}

fs::path appdata_dir() {
    auto env = std::getenv("LocalAppData");
    assert(env);

    return fs::absolute(fs::path(env));
}

}  // namespace

fs::path dds::user_data_dir() { return appdatalocal_dir(); }
fs::path dds::user_cache_dir() { return appdatalocal_dir(); }
fs::path dds::user_config_dir() { return appdata_dir(); }

#endif
