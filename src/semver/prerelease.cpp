#include "./prerelease.hpp"

using namespace semver;

prerelease prerelease::parse(std::string_view s) {
    auto ids = ident::parse_dotted_seq(s);
    for (auto& id : ids) {
        if (id.kind() == ident_kind::digits) {
            throw invalid_ident("Prerelease strings may not have plain-digit identifiers: "
                                + std::string(s));
        }
    }
    prerelease ret;
    ret._ids = std::move(ids);
    return ret;
}

order semver::compare(const prerelease& lhs, const prerelease& rhs) noexcept {
    auto       lhs_iter = lhs.idents().cbegin();
    auto       rhs_iter = rhs.idents().cbegin();
    const auto lhs_end  = lhs.idents().cend();
    const auto rhs_end  = rhs.idents().cend();

    for (; lhs_iter != lhs_end && rhs_iter != rhs_end; ++lhs_iter, ++rhs_iter) {
        auto ord = compare(*lhs_iter, *rhs_iter);
        if (ord != order::equivalent) {
            return ord;
        }
    }
    if (lhs_iter != lhs_end) {
        // Left-hand is longer
        return order::greater;
    } else if (rhs_iter != rhs_end) {
        // Right-hand is longer
        return order::less;
    } else {
        // They are equivalent
        return order::equivalent;
    }
}