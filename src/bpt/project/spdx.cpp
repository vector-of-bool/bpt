#include "./spdx.hpp"

#include <bpt/error/on_error.hpp>
#include <bpt/util/string.hpp>
#include <bpt/util/tl.hpp>
#include <bpt/util/wrap_var.hpp>

#include <boost/leaf/exception.hpp>
#include <neo/memory.hpp>
#include <neo/opt_ref.hpp>
#include <neo/ufmt.hpp>
#include <neo/utility.hpp>

#include <ranges>

using namespace bpt;

namespace {

const bpt::spdx_license_info ALL_LICENSES[] = {
#define SPDX_LICENSE(ID, Name) bpt::spdx_license_info{.id = ID, .name = Name},
#include "./spdx.inl"
};

const bpt::spdx_exception_info ALL_EXCEPTIONS[] = {
#define SPDX_EXCEPTION(ID, Name) bpt::spdx_exception_info{.id = ID, .name = Name},
#include "./spdx-exc.inl"
};

}  // namespace

const std::span<const bpt::spdx_license_info>   bpt::spdx_license_info::all   = ALL_LICENSES;
const std::span<const bpt::spdx_exception_info> bpt::spdx_exception_info::all = ALL_EXCEPTIONS;

namespace {

constexpr bool is_idstring_char(char32_t c) noexcept {
    if (neo::between(c, 'a', 'z') || neo::between(c, 'A', 'Z') || neo::between(c, '0', '9')) {
        return true;
    }
    if (c == '-' || c == '.') {
        return true;
    }
    return false;
}

std::string_view next_token(std::string_view sv) {
    sv      = bpt::trim_view(sv);
    auto it = sv.begin();
    if (it == sv.end()) {
        return sv;
    }
    if (*it == neo::oper::any_of('(', ')', '+')) {
        return sv.substr(0, 1);
    }
    while (it != sv.end() && is_idstring_char(*it)) {
        ++it;
    }
    return bpt::sview(sv.begin(), it);
}

template <typename T>
neo::opt_ref<const T> find_with_id(std::string_view id) {
    auto found = std::ranges::lower_bound(T::all, id, std::less<>{}, BPT_TL(_1.id));
    if (found == std::ranges::end(T::all) || found->id != id) {
        return std::nullopt;
    }
    return *found;
}

struct compound_expression;

struct just_license_id {
    spdx_license_info license;
};
struct license_id_plus {
    spdx_license_info license;
};
struct license_ref {};

struct simple_expression
    : bpt::variant_wrapper<just_license_id, license_id_plus /* , license_ref */> {
    using variant_wrapper::variant_wrapper;
    using variant_wrapper::visit;

    static simple_expression parse(std::string_view& sv) {
        sv           = bpt::trim_view(sv);
        auto tok     = next_token(sv);
        auto license = find_with_id<spdx_license_info>(tok);
        if (!license) {
            BOOST_LEAF_THROW_EXCEPTION(
                e_bad_spdx_expression{neo::ufmt("No such SPDX license '{}'", tok)});
        }
        sv.remove_prefix(tok.size());
        tok = next_token(sv);
        if (tok == "+") {
            sv.remove_prefix(1);
            return license_id_plus{*license};
        } else {
            return just_license_id{*license};
        }
    }
};

struct and_license {
    std::shared_ptr<compound_expression> left;
    std::shared_ptr<compound_expression> right;
};

struct or_license {
    std::shared_ptr<compound_expression> left;
    std::shared_ptr<compound_expression> right;
};

struct with_exception {
    simple_expression   license;
    spdx_exception_info exception;
};

struct paren_grouped {
    std::shared_ptr<compound_expression> expr;
};

struct compound_expression : bpt::variant_wrapper<simple_expression,
                                                  and_license,
                                                  or_license,
                                                  paren_grouped,
                                                  with_exception> {

    using variant_wrapper::variant_wrapper;
    using variant_wrapper::visit;

    static compound_expression parse(std::string_view& sv) {
        sv = bpt::trim_view(sv);
        if (next_token(sv) == "(") {
            sv.remove_prefix(1);
            auto inner = parse(sv);
            sv         = bpt::trim_view(sv);
            if (next_token(sv) != ")") {
                BOOST_LEAF_THROW_EXCEPTION(e_bad_spdx_expression{"Missing closing parenthesis"});
            }
            sv.remove_prefix(1);
            return paren_grouped{neo::copy_shared(inner)};
        }
        auto simple = simple_expression::parse(sv);
        sv          = bpt::trim_view(sv);
        auto next   = next_token(sv);
        if (next == "OR") {
            sv.remove_prefix(2);
            auto rhs = compound_expression::parse(sv);
            return or_license{neo::copy_shared(compound_expression{simple}), neo::copy_shared(rhs)};
        } else if (next == "AND") {
            sv.remove_prefix(3);
            auto rhs = compound_expression::parse(sv);
            return and_license{neo::copy_shared(compound_expression{simple}),
                               neo::copy_shared(rhs)};
        } else if (next == "WITH") {
            sv.remove_prefix(4);
            auto exc_id = next_token(sv);
            auto exc    = find_with_id<spdx_exception_info>(exc_id);
            if (!exc) {
                BOOST_LEAF_THROW_EXCEPTION(e_bad_spdx_expression{
                    neo::ufmt("No such SPDX license exception '{}'", exc_id)});
            }
            sv.remove_prefix(exc_id.size());
            return with_exception{std::move(simple), *exc};
        }
        return simple;
    }
};

struct license_to_string_fn {
    std::string str(just_license_id& l) { return l.license.id; }
    std::string str(license_id_plus& l) { return l.license.id + "+"; }

    std::string str(and_license& a) { return neo::ufmt("{} AND {}", str(*a.left), str(*a.right)); }

    std::string str(or_license& o) { return neo::ufmt("{} OR {}", str(*o.left), str(*o.right)); }

    std::string str(paren_grouped& o) { return neo::ufmt("({})", str(*o.expr)); }

    std::string str(simple_expression& e) { return e.visit(*this); }
    std::string str(compound_expression& e) { return e.visit(*this); }

    std::string str(with_exception& e) {
        return neo::ufmt("{} WITH {}", str(e.license), str(e.exception));
    }

    std::string str(spdx_exception_info& e) { return e.id; }

    std::string operator()(auto& l) { return str(l); }
};

}  // namespace

struct spdx_license_expression::impl {
    compound_expression expr;
};

spdx_license_expression spdx_license_expression::parse(std::string_view const sv) {
    BPT_E_SCOPE(e_spdx_license_str{std::string(sv)});

    auto sv1 = sv;
    auto r   = compound_expression::parse(sv1);
    if (!bpt::trim_view(sv1).empty()) {
        BOOST_LEAF_THROW_EXCEPTION(
            e_bad_spdx_expression{neo::ufmt("Unknown trailing string content '{}'", sv1)}, r);
    }
    return spdx_license_expression{neo::copy_shared(impl{r})};
}

std::string spdx_license_expression::to_string() const noexcept {
    return _impl->expr.visit(license_to_string_fn{});
}
