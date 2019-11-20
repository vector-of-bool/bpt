#pragma once

#include <semver/ident.hpp>
#include <semver/order.hpp>

#include <stdexcept>
#include <string_view>
#include <vector>

namespace semver {

class prerelease;
order compare(const prerelease& lhs, const prerelease& rhs) noexcept;

class prerelease {
    std::vector<ident> _ids;

public:
    prerelease() = default;

    [[nodiscard]] bool empty() const noexcept { return _ids.empty(); }

    void add_ident(ident id);
    void add_ident(std::string_view str) { add_ident(ident(str)); }

    auto& idents() const noexcept { return _ids; }

    static prerelease parse(std::string_view str);

#define DEF_OP(op, expr)                                                                           \
    inline friend bool operator op(const prerelease& lhs, const prerelease& rhs) noexcept {        \
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
};

}  // namespace semver
