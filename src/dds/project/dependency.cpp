#include "./dependency.hpp"

#include <dds/error/human.hpp>
#include <dds/error/on_error.hpp>
#include <dds/error/result.hpp>
#include <dds/util/json_walk.hpp>
#include <dds/util/parse_enum.hpp>
#include <dds/util/string.hpp>

#include <boost/leaf/exception.hpp>
#include <neo/assert.hpp>
#include <neo/utility.hpp>
#include <semver/range.hpp>

using namespace dds;

crs::dependency project_dependency::as_crs_dependency() const noexcept {
    return crs::dependency{
        .name                = dep_name,
        .acceptable_versions = acceptable_versions,
        .kind                = kind,
        .uses = explicit_uses ? crs::dependency_uses{crs::explicit_uses_list{*explicit_uses}}
                              : crs::dependency_uses{crs::implicit_uses_all{}},
    };
}

project_dependency project_dependency::parse_dep_range_shorthand(std::string_view const sv) {
    DDS_E_SCOPE(e_parse_dep_range_shorthand_string{std::string(sv)});

    project_dependency ret;

    auto sep_pos = sv.find_first_of("=@^~+");
    if (sep_pos == sv.npos) {
        BOOST_LEAF_THROW_EXCEPTION(
            e_human_message{"Expected an one of '=@^~+' in name+version shorthand"});
    }

    ret.dep_name = dds::name::from_string(sv.substr(0, sep_pos)).value();

    auto range_str = std::string(sv.substr(sep_pos));
    if (range_str.front() == '@') {
        range_str[0] = '^';
    }
    const semver::range rng = semver::range::parse_restricted(range_str);

    ret.acceptable_versions = {rng.low(), rng.high()};
    return ret;
}

static std::string_view next_token(std::string_view sv) noexcept {
    sv = trim_view(sv);
    if (sv.empty()) {
        return sv;
    }
    auto it = sv.begin();
    if (*it == ',') {
        return sv.substr(0, 1);
    }
    while (it != sv.end() && !std::isspace(*it) && *it != ',') {
        ++it;
    }
    auto len = it - sv.begin();
    return sv.substr(0, len);
}

project_dependency project_dependency::from_shorthand_string(const std::string_view sv) {
    DDS_E_SCOPE(e_parse_dep_shorthand_string{std::string(sv)});
    std::string_view remain    = sv;
    std::string_view tok       = remain.substr(0, 0);
    auto             adv_token = [&] {
        remain.remove_prefix(tok.data() - remain.data());
        remain.remove_prefix(tok.size());
        return tok = next_token(remain);
    };

    adv_token();
    if (tok.empty()) {
        BOOST_LEAF_THROW_EXCEPTION(e_human_message{"Invalid empty dependency specifier"});
    }

    auto ret = parse_dep_range_shorthand(tok);

    adv_token();
    if (tok.empty()) {
        return ret;
    }

    if (tok != neo::oper::any_of("for", "using", "")) {
        BOOST_LEAF_THROW_EXCEPTION(e_human_message{
            neo::ufmt("Expected 'for' or 'using' following dependency name and range (Got '{}')",
                      tok)});
    }

    if (tok == "using") {
        ret.explicit_uses.emplace();
        while (1) {
            adv_token();
            if (tok == ",") {
                BOOST_LEAF_THROW_EXCEPTION(
                    e_human_message{"Unexpected extra comma in dependency specifier"});
            }
            ret.explicit_uses->emplace_back(*dds::name::from_string(tok));
            if (adv_token() != ",") {
                break;
            }
        }
    }

    if (tok == "for") {
        tok      = adv_token();
        ret.kind = parse_enum_str<crs::usage_kind>(std::string(tok));
        tok      = adv_token();
    }

    if (tok == "using") {
        BOOST_LEAF_THROW_EXCEPTION(
            e_human_message{"'using' must appear before 'for' in dependency shorthand string"});
    }

    if (!tok.empty()) {
        BOOST_LEAF_THROW_EXCEPTION(e_human_message{
            neo::ufmt("Unexpected trailing string in dependency string \"{}\"", remain)});
    }

    return ret;
}