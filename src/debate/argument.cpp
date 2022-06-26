#include "./argument.hpp"

#include <neo/ufmt.hpp>
#include <neo/utility.hpp>

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
    auto        pref_spell   = preferred_spelling();
    auto        real_valname = !valname.empty()
               ? valname
               : (long_spellings.empty() ? "<value>" : ("<" + long_spellings[0] + ">"));
    if (is_positional()) {
        if (required) {
            if (can_repeat) {
                ret.append(neo::ufmt("{} [{} [...]]", real_valname, real_valname));
            } else {
                ret.append(real_valname);
            }
        } else {
            if (can_repeat) {
                ret.append(neo::ufmt("[{} [{} [...]]]", real_valname, real_valname));
            } else {
                ret.append(neo::ufmt("[{}]", real_valname));
            }
        }
    } else if (nargs != 0) {
        char sep_char = pref_spell.starts_with("--") ? '=' : ' ';
        if (required) {
            ret.append(neo::ufmt("{}{}{}", pref_spell, sep_char, real_valname));
            if (can_repeat) {
                ret.append(neo::ufmt(" [{}{}{} [...]]", pref_spell, sep_char, real_valname));
            }
        } else {
            if (can_repeat) {
                ret.append(neo::ufmt("[{}{}{} [{}{}{} [...]]]",
                                     pref_spell,
                                     sep_char,
                                     real_valname,
                                     pref_spell,
                                     sep_char,
                                     real_valname));
            } else {
                ret.append(neo::ufmt("[{}{}{}]", pref_spell, sep_char, real_valname));
            }
        }
    } else {
        ret.append(neo::ufmt("[{}]", pref_spell));
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

bool debate::parse_bool_string(std::string_view sv) {
    if (sv
        == neo::oper::any_of("y",  //
                             "Y",
                             "yes",
                             "YES",
                             "Yes",
                             "true",
                             "TRUE",
                             "True",
                             "on",
                             "ON",
                             "On",
                             "1")) {
        return true;
    } else if (sv
               == neo::oper::any_of("n",
                                    "N",
                                    "no",
                                    "NO",
                                    "No",
                                    "false",
                                    "FALSE",
                                    "False",
                                    "off",
                                    "OFF",
                                    "Off",
                                    "0")) {
        return false;
    } else {
        BOOST_LEAF_THROW_EXCEPTION(invalid_arguments("Invalid string for boolean/toggle option"),
                                   e_invalid_arg_value{std::string(sv)});
    }
}
