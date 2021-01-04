#ifdef _WIN32

#include "./paths.hpp"

#include <dds/util/log.hpp>

#include <cassert>
#include <cstdlib>

#include <windows.h>

using namespace dds;

namespace {

std::wstring
getenv_wstr(std::wstring varname, std::wstring default_val, std::size_t size_hint = 256) {
    std::wstring ret;
    ret.resize(size_hint);
    while (true) {
        auto real_len
            = ::GetEnvironmentVariableW(varname.data(), ret.data(), static_cast<DWORD>(ret.size()));
        if (real_len == 0 && ::GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
            // Environment variable is not defined
            return default_val;
        } else if (real_len > size_hint) {
            // Try again, with a larger buffer
            ret.resize(real_len);
            continue;
        } else {
            // Got it!
            ret.resize(real_len);
            return ret;
        }
    }
}

}  // namespace

fs::path dds::user_home_dir() {
    static auto ret = []() -> fs::path {
        std::wstring userprofile_env = getenv_wstr(L"UserProfile", L"/");
        return fs::absolute(fs::path(userprofile_env));
    }();
    return ret;
}

namespace {

fs::path appdatalocal_dir() {
    static auto env = getenv_wstr(L"LocalAppData", L"/");
    return fs::absolute(fs::path(env));
}

fs::path appdata_dir() {
    static auto env = getenv_wstr(L"AppData", L"/");
    return fs::absolute(fs::path(env));
}

}  // namespace

fs::path dds::user_data_dir() { return appdatalocal_dir(); }
fs::path dds::user_cache_dir() { return appdatalocal_dir(); }
fs::path dds::user_config_dir() { return appdata_dir(); }

#endif
