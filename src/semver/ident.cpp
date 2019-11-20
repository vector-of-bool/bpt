#include "./ident.hpp"

#include <cassert>
#include <charconv>

using namespace semver;

ident::ident(std::string_view str) {
    const auto str_begin = str.data();
    auto       ptr       = str_begin;
    const auto str_end   = str_begin + str.size();

    bool any_alpha = false;
    if (ptr == str_end) {
        throw invalid_ident(std::string(str));
    }
    for (; ptr != str_end; ++ptr) {
        any_alpha = any_alpha || (*ptr == '-' || std::isalpha(*ptr));
        if (!std::isalnum(*ptr) && *ptr != '-') {
            break;
        }
    }
    if (ptr != str_end) {
        throw invalid_ident(std::string(str));
    }

    _str.assign(str_begin, ptr);

    if (any_alpha) {
        _kind = ident_kind::alphanumeric;
    } else if (_str.size() > 1 && _str[0] == '0') {
        _kind = ident_kind::digits;
    } else {
        _kind = ident_kind::numeric;
        // Check that the integer is representable on this system
        std::uint64_t n;
        auto          res = std::from_chars(str_begin, str_end, n);
        if (res.ptr != str_end) {
            throw invalid_ident(_str);
        }
    }
}

std::vector<ident> ident::parse_dotted_seq(const std::string_view s) {
    std::vector<ident> acc;
    auto               remaining = s;

    while (!remaining.empty()) {
        auto next_dot = remaining.find('.');
        auto id_sub   = remaining.substr(0, next_dot);
        if (id_sub.empty()) {
            throw invalid_ident(std::string(s));
        }
        acc.emplace_back(id_sub);
        if (next_dot == remaining.npos) {
            break;
        }
        remaining = remaining.substr(next_dot + 1);
        if (remaining.empty()) {
            throw invalid_ident(std::string(s));
        }
    }
    if (acc.empty()) {
        throw invalid_ident(std::string(s));
    }
    return acc;
}

order semver::compare(ident_kind lhs, ident_kind rhs) noexcept {
    assert(lhs != ident_kind::digits && "Ordering with digit identifiers is undefined");
    assert(rhs != ident_kind::digits && "Ordering with digit identifiers is undefined");
    if (lhs == rhs) {
        return order::equivalent;
    } else if (lhs == ident_kind::numeric) {
        // numeric is less than alnum
        return order::less;
    } else {
        return order::greater;
    }
}

order semver::compare(const ident& lhs, const ident& rhs) noexcept {
    auto ord = compare(lhs.kind(), rhs.kind());
    if (ord != order::equivalent) {
        return ord;
    }
    assert(lhs.kind() == rhs.kind() && "[semver library bug]");
    if (lhs.kind() == ident_kind::numeric) {
        auto as_int = [](auto& str) {
            std::uint64_t value;
            auto          fc_res = std::from_chars(str.data(), str.data() + str.size(), value);
            assert(fc_res.ptr == str.data() + str.size());
            return value;
        };
        auto lhs_num = as_int(lhs.string());
        auto rhs_num = as_int(rhs.string());
        if (lhs_num == rhs_num) {
            return order::equivalent;
        } else if (lhs_num < rhs_num) {
            return order::less;
        } else {
            return order::greater;
        }
    }
    assert(lhs.kind() == ident_kind::alphanumeric && "[semver library bug]");
    auto comp = lhs.string().compare(rhs.string());
    if (comp == 0) {
        return order::equivalent;
    } else if (comp < 0) {
        return order::less;
    } else {
        return order::greater;
    }
}