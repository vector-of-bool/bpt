#pragma once

#include <dds/util/string.hpp>

#include <boost/leaf/exception.hpp>
#include <magic_enum.hpp>

namespace dds {

template <typename E>
struct e_invalid_enum {
    std::string value;
};

struct e_invalid_enum_str {
    std::string value;
};

struct e_enum_options {
    std::string value;
};

template <typename E>
constexpr auto parse_enum_str = [](const std::string& sv) {
    auto e = magic_enum::enum_cast<E>(sv);
    if (e.has_value()) {
        return *e;
    }

    BOOST_LEAF_THROW_EXCEPTION(  //
        e_invalid_enum<E>{std::string(magic_enum::enum_type_name<E>())},
        e_invalid_enum_str{std::string(sv)},
        [&] {
            auto        names = magic_enum::enum_names<E>();
            std::string acc
                = dds::joinstr(", ", names | std::views::transform([](auto n) {
                                         return std::string{"\""} + std::string(n) + "\"";
                                     }));
            return e_enum_options{acc};
        });
};

}  // namespace dds