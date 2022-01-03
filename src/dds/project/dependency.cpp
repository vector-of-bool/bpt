#include "./dependency.hpp"

#include <dds/deps.hpp>
#include <dds/error/human.hpp>
#include <dds/error/on_error.hpp>
#include <dds/error/result.hpp>
#include <dds/util/json_walk.hpp>
#include <dds/util/parse_enum.hpp>
#include <dds/util/string.hpp>

#include <boost/leaf/exception.hpp>
#include <magic_enum.hpp>
#include <neo/assert.hpp>
#include <neo/ranges.hpp>
#include <neo/tl.hpp>
#include <neo/utility.hpp>

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

project_dependency project_dependency::parse_dep_range_shorthand(std::string_view s) {
    auto               legacy = dds::dependency::parse_depends_string(s);
    project_dependency ret;
    ret.dep_name            = legacy.name;
    ret.acceptable_versions = legacy.versions;
    // These are numeric compatible:
    ret.kind = static_cast<crs::usage_kind>(static_cast<int>(legacy.for_kind));
    neo_assert(invariant, !legacy.uses.has_value(), "What");
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
        auto off   = tok.end() - remain.begin();
        remain     = remain.substr(off);
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

    if (tok != neo::oper::any_of("for", "uses", "")) {
        BOOST_LEAF_THROW_EXCEPTION(
            e_human_message{"Expected 'for' or 'uses' following dependency name and range"});
    }

    if (tok == "uses") {
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

    if (tok == "uses") {
        BOOST_LEAF_THROW_EXCEPTION(
            e_human_message{"'uses' must appear before 'for' in dependency shorthand string"});
    }

    if (!tok.empty()) {
        BOOST_LEAF_THROW_EXCEPTION(e_human_message{
            neo::ufmt("Unexpected trailing string in dependency string \"{}\"", remain)});
    }

    return ret;
}