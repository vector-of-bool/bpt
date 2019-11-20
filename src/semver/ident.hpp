#pragma once

#include <semver/order.hpp>

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace semver {

class invalid_ident : public std::runtime_error {
    std::string _str;

public:
    invalid_ident(std::string s)
        : std::runtime_error::runtime_error("Invalid metadata identifier: " + s)
        , _str(s) {}

    auto& string() const noexcept { return _str; }
};

enum class ident_kind {
    alphanumeric,
    numeric,
    digits,
};

order compare(ident_kind lhs, ident_kind rhs) noexcept;

class ident;
order compare(const ident& lhs, const ident& rhs) noexcept;

class ident {
    std::string _str;
    ident_kind  _kind;

public:
    explicit ident(std::string_view str);

    auto        kind() const noexcept { return _kind; }
    const auto& string() const noexcept { return _str; }

#define DEF_OP(op, expr)                                                                           \
    inline friend bool operator op(const ident& lhs, const ident& rhs) noexcept {                  \
        auto o = compare(lhs, rhs);                                                                \
        return (expr);                                                                             \
    }                                                                                              \
    static_assert(true)

    DEF_OP(==, (o == order::equivalent));
    DEF_OP(!=, (o != order::equivalent));
    DEF_OP(<, (o == order::less));
    DEF_OP(>, (o == order::greater));
    DEF_OP(<=, (o == order::less || o == order::equivalent));
    DEF_OP(>=, (o == order::greater || o == order::equivalent));
#undef DEF_OP

    static std::vector<ident> parse_dotted_seq(std::string_view s);
};

}  // namespace semver