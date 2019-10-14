#include "./proc.hpp"

#include <dds/util.hpp>

#include <algorithm>
#include <array>
#include <cctype>

using namespace dds;

bool dds::needs_quoting(std::string_view s) {
    std::string_view okay_chars = "@%-+=:,./|_";
    const bool all_okay = std::all_of(s.begin(), s.end(), [&](char c) {
        return std::isalnum(c) || (okay_chars.find(c) != okay_chars.npos);
    });
    return !all_okay;
}

std::string dds::quote_argument(std::string_view s) {
    if (!needs_quoting(s)) {
        return std::string(s);
    }
    auto new_s = replace(s, "\\", "\\\\");
    new_s      = replace(s, "\"", "\\\"");
    return "\"" + new_s + "\"";
}
