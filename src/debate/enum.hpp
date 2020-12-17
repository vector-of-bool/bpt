#pragma once

#include "./argument_parser.hpp"
#include "./error.hpp"

#include <boost/leaf/error.hpp>
#include <boost/leaf/exception.hpp>
#include <fmt/core.h>
#include <magic_enum.hpp>

#include <type_traits>

namespace debate {

template <typename E>
class enum_putter {
    E* _dest;

public:
    constexpr explicit enum_putter(E& e)
        : _dest(&e) {}

    void operator()(std::string_view given, std::string_view full_arg) const {
        std::optional<std::string> normalized_str;
        std::string_view           normalized_view = given;
        if (given.find('-') != given.npos) {
            // We should normalize it
            normalized_str.emplace(given);
            for (char& c : *normalized_str) {
                c = c == '-' ? '_' : c;
            }
            normalized_view = *normalized_str;
        }
        auto val = magic_enum::enum_cast<E>(normalized_view);
        if (!val) {
            throw boost::leaf::
                exception(invalid_arguments("Invalid argument value given for enum-bound argument"),
                          e_invalid_arg_value{std::string(given)},
                          e_arg_spelling{std::string(full_arg)});
        }
        *_dest = *val;
    }
};

template <typename E>
constexpr auto make_enum_putter(E& dest) noexcept {
    return enum_putter<E>(dest);
}

}  // namespace debate
