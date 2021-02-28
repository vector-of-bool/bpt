#include "./name.hpp"

#include <dds/error/on_error.hpp>
#include <dds/error/result.hpp>

#include <neo/assert.hpp>

#include <ctre.hpp>

using namespace dds;

using err_reason = invalid_name_reason;

static err_reason calc_invalid_name_reason(std::string_view str) noexcept {
    constexpr ctll::fixed_string capital_re    = "[A-Z]";
    constexpr ctll::fixed_string double_punct  = "[._\\-]{2}";
    constexpr ctll::fixed_string end_punct     = "[._\\-]$";
    constexpr ctll::fixed_string ws            = "\\s";
    constexpr ctll::fixed_string invalid_chars = "[^a-z0-9._\\-]";
    if (str.empty()) {
        return err_reason::empty;
    } else if (ctre::search<capital_re>(str)) {
        return err_reason::capital;
    } else if (ctre::search<double_punct>(str)) {
        return err_reason::double_punct;
    } else if (str[0] < 'a' || str[0] > 'z') {
        return err_reason::initial_not_alpha;
    } else if (ctre::search<end_punct>(str)) {
        return err_reason::end_punct;
    } else if (ctre::search<ws>(str)) {
        return err_reason::whitespace;
    } else if (ctre::search<invalid_chars>(str)) {
        return err_reason::invalid_char;
    } else {
        neo_assert(invariant,
                   false,
                   "Expected to be able to determine an error-reason for the given invalid name",
                   str);
    }
}

std::string_view dds::invalid_name_reason_str(err_reason e) noexcept {
    switch (e) {
    case err_reason::capital:
        return "Uppercase letters are not valid in names";
    case err_reason::double_punct:
        return "Adjacent punctuation characters are not valid in names";
    case err_reason::end_punct:
        return "Names must not end with a punctuation character";
    case err_reason::whitespace:
        return "Names must not contain whitespace";
    case err_reason::invalid_char:
        return "Name contains an invalid character";
    case err_reason::initial_not_alpha:
        return "Name must begin with a lowercase alphabetic character";
    case err_reason::empty:
        return "Name cannot be empty";
    }
    neo::unreachable();
}

result<name> name::from_string(std::string_view str) noexcept {
    constexpr ctll::fixed_string name_re = "^([a-z][a-z0-9]*)([._\\-][a-z0-9]+)*$";

    auto mat = ctre::match<name_re>(str);

    if (!mat) {
        return new_error(DDS_E_ARG(e_name_str{std::string(str)}),
                         DDS_E_ARG(calc_invalid_name_reason(str)));
    }

    return name{std::string(str)};
}

std::ostream& dds::operator<<(std::ostream& out, invalid_name_reason r) {
    out << invalid_name_reason_str(r);
    return out;
}