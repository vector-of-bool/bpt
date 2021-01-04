#include "./argument.hpp"

#include <fmt/color.h>

using namespace debate;

using strv = std::string_view;

using namespace std::literals;

strv argument::try_match_short(strv given) const noexcept {
    for (auto& cand : short_spellings) {
        if (given.starts_with(cand)) {
            return cand;
        }
    }
    return "";
}

strv argument::try_match_long(strv given) const noexcept {
    for (auto& cand : long_spellings) {
        if (!given.starts_with(cand)) {
            continue;
        }
        auto tail = given.substr(cand.size());
        // We should either be empty, as in '--argument value',
        // or followed by an equal, as in '--argument=value'
        if (tail.empty() || tail[0] == '=') {
            return cand;
        }
    }
    return "";
}

std::string argument::preferred_spelling() const noexcept {
    if (!long_spellings.empty()) {
        return "--"s + long_spellings.front();
    } else if (!short_spellings.empty()) {
        return "-"s + short_spellings.front();
    } else {
        return valname;
    }
}

std::string argument::syntax_string() const noexcept {
    std::string ret;
    if (!required) {
        ret.push_back('[');
    }
    if (is_positional()) {
        ret.append(preferred_spelling());
    } else {
        ret.append(preferred_spelling());
        if (nargs != 0) {
            auto real_valname = !valname.empty()
                ? valname
                : (long_spellings.empty() ? "<value>" : ("<" + long_spellings[0] + ">"));
            ret.append(fmt::format(" {}", valname.empty() ? "<value>" : valname));
        }
    }
    if (can_repeat) {
        ret.append(" ...");
    }
    if (!required) {
        ret.push_back(']');
    }
    return ret;
}

std::string argument::help_string() const noexcept {
    std::string ret;
    for (auto& l : long_spellings) {
        ret.append(fmt::format(fmt::emphasis::bold, "--{}", l));
        if (nargs != 0) {
            ret.append(
                fmt::format(fmt::emphasis::italic, "={}", valname.empty() ? "<value>" : valname));
        }
        ret.push_back('\n');
    }
    for (auto& s : short_spellings) {
        ret.append(fmt::format(fmt::emphasis::bold, "-{}", s));
        if (nargs != 0) {
            ret.append(
                fmt::format(fmt::emphasis::italic, " {}", valname.empty() ? "<value>" : valname));
        }
        ret.push_back('\n');
    }
    if (is_positional()) {
        ret.append(preferred_spelling() + "\n");
    }
    ret.append("  ");
    for (auto c : help) {
        ret.push_back(c);
        if (c == '\n') {
            ret.append(2, ' ');
        }
    }
    ret.push_back('\n');

    return ret;
}
