#pragma once

#include "./argument_parser.hpp"
#include "./error.hpp"

#include <boost/leaf/error.hpp>
#include <boost/leaf/exception.hpp>
#include <fmt/core.h>
#include <magic_enum.hpp>

#include <algorithm>
#include <type_traits>

namespace debate {

template <typename E>
class enum_putter {
    E* _dest;

    static std::string _kebab_name(std::string val_ident) {
        std::ranges::replace(val_ident, '_', '-');
        auto trim_pos = val_ident.find_last_not_of("-");
        if (trim_pos != std::string::npos) {
            val_ident.erase(trim_pos + 1);
        }
        return val_ident;
    }

public:
    constexpr explicit enum_putter(E& e)
        : _dest(&e) {}

    void operator()(std::string_view given, std::string_view full_arg) const {
        auto entries  = magic_enum::enum_entries<E>();
        auto matching = std::ranges::find(entries, given, [](auto&& p) {
            return _kebab_name(std::string(p.second));
        });
        if (matching == std::ranges::end(entries)) {
            throw boost::leaf::
                exception(invalid_arguments("Invalid argument value given for enum-bound argument"),
                          e_invalid_arg_value{std::string(given)},
                          e_arg_spelling{std::string(full_arg)});
        }

        *_dest = matching->first;
    }
};

template <typename E>
constexpr auto make_enum_putter(E& dest) noexcept {
    return enum_putter<E>(dest);
}

}  // namespace debate
