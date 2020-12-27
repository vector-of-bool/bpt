#include "./argument_parser.hpp"

#include <boost/leaf/error.hpp>
#include <boost/leaf/exception.hpp>
#include <boost/leaf/on_error.hpp>

#include <fmt/color.h>
#include <fmt/format.h>

#include <set>

using strv = std::string_view;

using namespace debate;

namespace {

struct parse_engine {
    debate::detail::parser_state& state;
    const argument_parser*        bottom_parser;

    // Keep track of how many positional arguments we have seen
    int positional_index = 0;

    // Keep track of what we've seen
    std::set<const argument*> seen{};

    auto current_arg() const noexcept { return state.current_arg(); }
    auto at_end() const noexcept { return state.at_end(); }
    void shift() noexcept { return state.shift(); }

    void see(const argument& arg) {
        auto did_insert = seen.insert(&arg).second;
        if (!did_insert && !arg.can_repeat) {
            throw boost::leaf::exception(invalid_repitition("Invalid repitition"));
        }
    }

    void run() {
        auto _ = boost::leaf::on_error([this] { return e_argument_parser{*bottom_parser}; });
        while (!at_end()) {
            parse_another();
        }
        // Parsed everything successfully. Cool.
        finalize();
    }

    void parse_another() {
        auto given     = current_arg();
        auto did_parse = try_parse_given(given);
        if (!did_parse) {
            throw boost::leaf::exception(unrecognized_argument("Unrecognized argument"),
                                         e_arg_spelling{std::string(given)});
        }
    }

    bool try_parse_given(const strv given) {
        if (given.size() < 2 || given[0] != '-') {
            if (try_parse_positional(given)) {
                return true;
            }
            return try_parse_subparser(given);
        } else if (given[1] == '-') {
            // Two hyphens is a long argument
            return try_parse_long(given.substr(2), given);
        } else {
            // A single hyphen, shorthand argument(s)
            return try_parse_short(given.substr(1), given);
        }
    }

    /*
    ##        #######  ##    ##  ######
    ##       ##     ## ###   ## ##    ##
    ##       ##     ## ####  ## ##
    ##       ##     ## ## ## ## ##   ####
    ##       ##     ## ##  #### ##    ##
    ##       ##     ## ##   ### ##    ##
    ########  #######  ##    ##  ######
    */

    bool try_parse_long(strv tail, const strv given) {
        if (tail == "help") {
            throw boost::leaf::exception(help_request());
        }
        auto argset = bottom_parser;
        while (argset) {
            if (try_parse_long_1(*argset, tail, given)) {
                return true;
            }
            argset = argset->parent().pointer();
        }
        return false;
    }

    bool try_parse_long_1(const argument_parser& argset, strv tail, const strv) {
        for (const argument& cand : argset.arguments()) {
            auto matched = cand.try_match_long(tail);
            if (matched.empty()) {
                continue;
            }
            tail.remove_prefix(matched.size());
            shift();
            auto long_arg = fmt::format("--{}", matched);
            auto _        = boost::leaf::on_error(e_argument{cand}, e_arg_spelling{long_arg});
            see(cand);
            return dispatch_long(cand, tail, long_arg);
        }

        // None of the arguments matched
        return false;
    }

    bool dispatch_long(const argument& arg, strv tail, strv given) {
        if (arg.nargs == 0) {
            if (!tail.empty()) {
                // We should not have a value
                throw boost::leaf::exception(invalid_arguments("Argument does not expect a value"),
                                             e_wrong_val_num{1});
            }
            // Just a switch. Dispatch
            arg.action(given, given);
            return true;
        }
        // We expect at least one value
        neo_assert(invariant,
                   tail.empty() || tail[0] == '=',
                   "Invalid argparsing state",
                   tail,
                   given);
        if (!tail.empty()) {
            // Given with an '=', as in: '--long-option=value'
            tail.remove_prefix(1);
            // The remainder is a single value
            if (arg.nargs > 1) {
                throw boost::leaf::exception(invalid_arguments("Invalid number of values"),
                                             e_wrong_val_num{1});
            }
            arg.action(tail, given);
        } else {
            // Trailing words are arguments
            for (auto i = 0; i < arg.nargs; ++i) {
                if (at_end()) {
                    throw boost::leaf::exception(invalid_arguments(
                                                     "Invalid number of argument values"),
                                                 e_wrong_val_num{i});
                }
                arg.action(current_arg(), given);
                shift();
            }
        }
        return true;
    }

    /*
     ######  ##     ##  #######  ########  ########
    ##    ## ##     ## ##     ## ##     ##    ##
    ##       ##     ## ##     ## ##     ##    ##
     ######  ######### ##     ## ########     ##
          ## ##     ## ##     ## ##   ##      ##
    ##    ## ##     ## ##     ## ##    ##     ##
     ######  ##     ##  #######  ##     ##    ##
    */

    bool try_parse_short(strv tail, const strv given) {
        if (tail == "h") {
            throw boost::leaf::exception(help_request());
        }
        auto argset = bottom_parser;
        while (argset) {
            auto new_tail = try_parse_short_1(*argset, tail, given);
            if (new_tail == tail) {
                // No characters were consumed...
                argset = argset->parent().pointer();
            } else {
                // Got one argument. Re-seek back to the bottom-most active parser
                argset = bottom_parser;
                tail   = new_tail;
            }
            if (tail.empty()) {
                // We parsed the full group
                return true;
            }
        }
        // Did not match anything...
        return false;
    }

    strv try_parse_short_1(const argument_parser& argset, const strv tail, const strv) {
        for (const argument& cand : argset.arguments()) {
            auto matched = cand.try_match_short(tail);
            if (matched.empty()) {
                continue;
            }
            auto        short_tail = tail.substr(matched.size());
            std::string short_arg  = fmt::format("-{}", matched);
            auto        _ = boost::leaf::on_error(e_argument{cand}, e_arg_spelling{short_arg});
            see(cand);
            return dispatch_short(cand, short_tail, short_arg);
        }
        // Didn't match anything. Return the original group unmodified
        return tail;
    }

    strv dispatch_short(const argument& arg, strv tail, strv spelling) {
        if (!arg.nargs) {
            // Just a switch. Consume a single character
            arg.action("", spelling);
            return tail;
        } else if (arg.nargs == 1) {
            // Want one value
            if (tail.empty()) {
                // The next argument is the value
                shift();
                if (at_end()) {
                    throw boost::leaf::exception(invalid_arguments("Expected a value"));
                }
                arg.action(current_arg(), spelling);
                shift();
                // We consumed the whole group, so return empty as the remaining:
                return "";
            } else {
                // Consume the remainder of the argument as the value
                arg.action(tail, spelling);
                shift();
                return "";
            }
        } else {
            // Consume the next arguments
            if (!tail.empty()) {
                throw boost::leaf::exception(invalid_arguments(
                                                 "Wrong number of argument values given"),
                                             e_wrong_val_num{1});
            }
            shift();
            for (auto i = 0; i < arg.nargs; ++i) {
                if (at_end()) {
                    throw boost::leaf::exception(invalid_arguments(
                                                     "Wrong number of argument values"),
                                                 e_wrong_val_num{i});
                }
                arg.action(current_arg(), spelling);
                shift();
            }
            return "";
        }
    }

    /*
    ########   #######   ######  #### ######## ####  #######  ##    ##    ###    ##
    ##     ## ##     ## ##    ##  ##     ##     ##  ##     ## ###   ##   ## ##   ##
    ##     ## ##     ## ##        ##     ##     ##  ##     ## ####  ##  ##   ##  ##
    ########  ##     ##  ######   ##     ##     ##  ##     ## ## ## ## ##     ## ##
    ##        ##     ##       ##  ##     ##     ##  ##     ## ##  #### ######### ##
    ##        ##     ## ##    ##  ##     ##     ##  ##     ## ##   ### ##     ## ##
    ##         #######   ######  ####    ##    ####  #######  ##    ## ##     ## ########
    */

    bool try_parse_positional(strv given) {
        int pos_idx = 0;
        for (auto& arg : bottom_parser->arguments()) {
            if (!arg.is_positional()) {
                continue;
            }

            if (pos_idx != this->positional_index) {
                // Not yet
                ++pos_idx;
                continue;
            }
            // We've found the next one that needs a value
            neo_assert(expects,
                       arg.nargs == 1,
                       "Positional arguments must have their nargs=1. For more than one "
                       "positional, use multiple positional arguments objects.",
                       arg.nargs,
                       given,
                       positional_index);
            // Just invoke the action
            auto _ = boost::leaf::on_error(e_arg_spelling{arg.preferred_spelling()});
            see(arg);
            arg.action(given, given);
            if (!arg.can_repeat) {
                // This argument isn't repeatable. Advance past it
                ++this->positional_index;
                // If an arg is repeatable, it will always be the "next positional" to parse,
                // and subsequent positionals are inherently unreachable.
            }
            shift();
            return true;
        }
        // No one accepted the value. We do not follow the chain of subcommands for positionals
        return false;
    }

    /*
     ######  ##     ## ########  ########     ###    ########   ######  ######## ########
    ##    ## ##     ## ##     ## ##     ##   ## ##   ##     ## ##    ## ##       ##     ##
    ##       ##     ## ##     ## ##     ##  ##   ##  ##     ## ##       ##       ##     ##
     ######  ##     ## ########  ########  ##     ## ########   ######  ######   ########
          ## ##     ## ##     ## ##        ######### ##   ##         ## ##       ##   ##
    ##    ## ##     ## ##     ## ##        ##     ## ##    ##  ##    ## ##       ##    ##
     ######   #######  ########  ##        ##     ## ##     ##  ######  ######## ##     ##
    */

    bool try_parse_subparser(const strv given) {
        if (!bottom_parser->subparsers()) {
            return false;
        }
        auto& group = *bottom_parser->subparsers();
        for (auto& cand : group._p_subparsers) {
            if (cand.name == given) {
                // This parser is now the bottom of the chain
                if (group.action) {
                    group.action(given, group.valname);
                }
                if (cand.action) {
                    cand.action();
                }
                bottom_parser    = &cand._p_parser;
                positional_index = 0;
                shift();
                return true;
            }
        }
        return false;
    }

    /*
    ######## #### ##    ##    ###    ##       #### ######## ########
    ##        ##  ###   ##   ## ##   ##        ##       ##  ##
    ##        ##  ####  ##  ##   ##  ##        ##      ##   ##
    ######    ##  ## ## ## ##     ## ##        ##     ##    ######
    ##        ##  ##  #### ######### ##        ##    ##     ##
    ##        ##  ##   ### ##     ## ##        ##   ##      ##
    ##       #### ##    ## ##     ## ######## #### ######## ########
    */

    void finalize() {
        auto argset = bottom_parser;
        while (argset) {
            finalize(*argset);
            argset = argset->parent().pointer();
        }
        if (bottom_parser->subparsers() && bottom_parser->subparsers()->required) {
            throw boost::leaf::exception(missing_required("Expected a subcommand"));
        }
    }

    void finalize(const argument_parser& argset) {
        for (auto& arg : argset.arguments()) {
            if (arg.required && !seen.contains(&arg)) {
                throw boost::leaf::exception(missing_required("Required argument is missing"),
                                             e_argument{arg});
            }
        }
    }
};

}  // namespace

void debate::detail::parser_state::run(const argument_parser& bottom) {
    parse_engine{*this, &bottom}.run();
}

argument& argument_parser::add_argument(argument arg) noexcept {
    _arguments.push_back(std::move(arg));
    return _arguments.back();
}

subparser_group& argument_parser::add_subparsers(subparser_group grp) noexcept {
    _subparsers.emplace(std::move(grp));
    _subparsers->_p_parent_ = this;
    return *_subparsers;
}

argument_parser& subparser_group::add_parser(subparser sub) {
    _p_subparsers.push_back(std::move(sub));
    auto& p   = _p_subparsers.back()._p_parser;
    p._parent = _p_parent_;
    return p;
}

std::string argument_parser::usage_string(std::string_view progname) const noexcept {
    std::string subcommand_suffix;
    auto        tail_parser = this;
    while (tail_parser) {
        for (auto& arg : tail_parser->arguments()) {
            if (arg.is_positional() && arg.required && tail_parser != this) {
                subcommand_suffix = " " + arg.preferred_spelling() + subcommand_suffix;
            }
        }
        if (!tail_parser->_name.empty()) {
            subcommand_suffix = " " + tail_parser->_name + subcommand_suffix;
        }
        tail_parser = tail_parser->_parent.pointer();
    }
    auto ret    = fmt::format("Usage: {}{}", progname, subcommand_suffix);
    auto indent = ret.size() + 1;
    if (indent > 40) {
        ret.push_back('\n');
        indent = 10;
        ret.append(indent, ' ');
    }

    std::size_t col = indent;
    for (auto& arg : _arguments) {
        auto synstr = arg.syntax_string();
        if (col + synstr.size() > 79 && col > indent) {
            ret.append("\n");
            ret.append(indent - 1, ' ');
            col = indent - 1;
        }
        ret.append(" " + synstr);
        col += synstr.size() + 1;
    }

    if (subparsers()) {
        std::string subcommand_str = " {";
        auto&       subs           = subparsers()->_p_subparsers;
        for (auto it = subs.cbegin(); it != subs.cend();) {
            subcommand_str.append(it->name);
            ++it;
            if (it != subs.cend()) {
                subcommand_str.append(",");
            }
        }
        subcommand_str.append("}");
        if (col + subcommand_str.size() > 79 && col > indent) {
            ret.append("\n");
            ret.append(indent - 1, ' ');
        }
        ret.append(subcommand_str);
    }
    return ret;
}

std::string argument_parser::help_string(std::string_view progname) const noexcept {
    std::string ret;
    ret = usage_string(progname);
    ret.append("\n\n");
    if (!_description.empty()) {
        ret.append(_description);
        ret.append("\n\n");
    }
    bool any_required = false;
    for (auto& arg : arguments()) {
        if (!arg.required) {
            continue;
        }
        if (!any_required) {
            ret.append("required arguments:\n\n");
        }
        any_required = true;
        ret.append(arg.help_string());
        ret.append("\n");
    }
    bool any_non_required = false;
    for (auto& arg : arguments()) {
        if (arg.required) {
            continue;
        }
        if (!any_non_required) {
            ret.append("optional arguments:\n\n");
        }
        any_non_required = true;
        ret.append(arg.help_string());
        ret.append("\n");
    }

    if (subparsers()) {
        ret.append("Subcommands:\n\n");
        if (!subparsers()->description.empty()) {
            ret.append(fmt::format("  {}\n\n", subparsers()->description));
        }
        for (auto& sub : subparsers()->_p_subparsers) {
            ret.append(fmt::format(fmt::emphasis::bold, "{}", sub.name));
            ret.append("\n  ");
            ret.append(sub.help);
            ret.append("\n\n");
        }
    }
    return ret;
}
