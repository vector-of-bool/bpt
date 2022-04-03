#include "./env.hpp"

#include <neo/utility.hpp>

#include <cstdlib>

std::optional<std::string> dds::getenv(const std::string& varname) noexcept {
    auto cptr = std::getenv(varname.data());
    if (cptr) {
        return std::string(cptr);
    }
    return {};
}

bool dds::getenv_bool(const std::string& varname) noexcept {
    auto s = getenv(varname);
    return s == neo::oper::any_of("1", "true", "on", "TRUE", "ON", "YES", "yes");
}
