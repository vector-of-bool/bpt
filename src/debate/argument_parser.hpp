#pragma once

#include "./argument.hpp"

#include <fmt/format.h>
#include <neo/assert.hpp>
#include <neo/opt_ref.hpp>

#include <cassert>
#include <exception>
#include <functional>
#include <list>

namespace debate {

class argument_parser;

namespace detail {

struct parser_state {
    void run(const argument_parser& bottom_parser);

    virtual std::string_view current_arg() const noexcept = 0;
    virtual bool             at_end() const noexcept      = 0;
    virtual void             shift() noexcept             = 0;
};

template <typename Iter, typename Stop>
struct parser_state_impl : parser_state {
    Iter arg_it;
    Stop arg_stop;

    parser_state_impl(Iter it, Stop st)
        : arg_it(it)
        , arg_stop(st) {}

    bool             at_end() const noexcept override { return arg_it == arg_stop; }
    std::string_view current_arg() const noexcept override {
        neo_assert(invariant, !at_end(), "Get argument past the final argumetn?");
        return *arg_it;
    }
    void shift() noexcept override {
        neo_assert(invariant, !at_end(), "Advancing argv parser past the end.");
        ++arg_it;
    }
};

}  // namespace detail

struct subparser;

struct subparser_group {
    std::string valname = "<subcommand>";

    std::string description{};

    bool required = true;

    std::function<void(std::string_view, std::string_view)> action{};

    const argument_parser* _p_parent_ = nullptr;
    std::list<subparser>   _p_subparsers{};

    argument_parser& add_parser(subparser);
};

class argument_parser {
    friend struct subparser_group;
    std::list<argument>            _arguments;
    std::optional<subparser_group> _subparsers;
    std::string                    _name;
    std::string                    _description;
    // The parent of this argumetn parser, if it was attached using a subparser_group
    neo::opt_ref<const argument_parser> _parent;

    using strv     = std::string_view;
    using str_iter = strv::iterator;

    template <typename R>
    void _parse_argv(R&& range) const {
        auto arg_it   = std::cbegin(range);
        auto arg_stop = std::cend(range);
        // Instantiate a complete parser, and go!
        detail::parser_state_impl state{arg_it, arg_stop};
        state.run(*this);
    }

public:
    argument_parser() = default;

    explicit argument_parser(std::string description)
        : _description(std::move(description)) {}

    explicit argument_parser(std::string name, std::string description)
        : _name(std::move(name))
        , _description(std::move(description)) {}

    argument& add_argument(argument arg) noexcept;

    subparser_group& add_subparsers(subparser_group grp = {}) noexcept;

    std::string usage_string(std::string_view progname) const noexcept;

    std::string help_string(std::string_view progname) const noexcept;

    template <typename T>
    void parse_argv(T&& range) const {
        return _parse_argv(range);
    }

    template <typename T>
    void parse_argv(std::initializer_list<T> ilist) const {
        return _parse_argv(ilist);
    }

    auto  parent() const noexcept { return _parent; }
    auto& arguments() const noexcept { return _arguments; }
    auto& subparsers() const noexcept { return _subparsers; }
};

struct subparser {
    std::string name;
    std::string help;

    std::function<void()> action{};

    argument_parser _p_parser{name, help};
};

}  // namespace debate
