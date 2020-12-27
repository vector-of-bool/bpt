#pragma once

#include "./error.hpp"

#include <boost/leaf/exception.hpp>
#include <fmt/format.h>

#include <charconv>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace debate {

template <typename E>
constexpr auto make_enum_putter(E& dest) noexcept;

template <typename T>
class argument_value_putter {
    T& _dest;

public:
    explicit argument_value_putter(T& dest) noexcept
        : _dest(dest) {}

    void operator()(std::string_view value, std::string_view) { _dest = T(value); }
};

template <typename Int>
class integer_putter {
    Int& _dest;

public:
    explicit integer_putter(Int& d)
        : _dest(d) {}

    void operator()(std::string_view value, std::string_view spelling) {
        auto res = std::from_chars(value.data(), value.data() + value.size(), _dest);
        if (res.ec != std::errc{} || res.ptr != value.data() + value.size()) {
            throw boost::leaf::exception(invalid_arguments(
                                             "Invalid value given for integral argument"),
                                         e_arg_spelling{std::string(spelling)},
                                         e_invalid_arg_value{std::string(value)});
        }
    }
};

template <typename T>
constexpr auto make_argument_putter(T& dest) {
    if constexpr (std::is_enum_v<T>) {
        return make_enum_putter(dest);  /// !! README: Include <debate/enum.hpp> to use enums here
    } else if constexpr (std::is_integral_v<T>) {
        return integer_putter(dest);
    } else {
        return argument_value_putter{dest};
    }
}

constexpr inline auto store_value = [](auto& dest, auto val) {
    return [&dest, val](std::string_view = {}, std::string_view = {}) { dest = val; };
};

constexpr inline auto store_true  = [](auto& dest) { return store_value(dest, true); };
constexpr inline auto store_false = [](auto& dest) { return store_value(dest, false); };

constexpr inline auto put_into = [](auto& dest) { return make_argument_putter(dest); };

constexpr inline auto push_back_onto = [](auto& dest) {
    return [&dest](std::string_view value, std::string_view = {}) { dest.emplace_back(value); };
};

struct argument {
    std::vector<std::string> long_spellings{};
    std::vector<std::string> short_spellings{};

    std::string help{};
    std::string valname{};

    bool required   = false;
    int  nargs      = 1;
    bool can_repeat = false;

    std::function<void(std::string_view, std::string_view)> action;

    // This member variable makes this strunct noncopyable, and has no other purpose
    std::unique_ptr<int> _make_noncopyable{};
    std::string_view     try_match_short(std::string_view arg) const noexcept;
    std::string_view     try_match_long(std::string_view arg) const noexcept;
    std::string          preferred_spelling() const noexcept;
    std::string          syntax_string() const noexcept;
    std::string          help_string() const noexcept;
    bool                 is_positional() const noexcept {
        return long_spellings.empty() && short_spellings.empty();
    }

    argument dup() const noexcept {
        return argument{
            .long_spellings  = long_spellings,
            .short_spellings = short_spellings,
            .help            = help,
            .valname         = valname,
            .required        = required,
            .nargs           = nargs,
            .can_repeat      = can_repeat,
            .action          = action,
        };
    }
};

}  // namespace debate