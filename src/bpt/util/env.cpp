#include "./env.hpp"

#include <neo/utility.hpp>

#include <cstdlib>

std::optional<std::string> bpt::getenv(const std::string& varname) noexcept {
    auto cptr = std::getenv(varname.data());
    if (cptr) {
        return std::string(cptr);
    }
    return {};
}

bool bpt::getenv_bool(const std::string& varname) noexcept {
    auto s = getenv(varname);
    return s.has_value() && is_truthy_string(*s);
}

bool bpt::is_truthy_string(std::string_view s) noexcept {
    return s == neo::oper::any_of("1", "true", "on", "TRUE", "ON", "YES", "yes");
}
