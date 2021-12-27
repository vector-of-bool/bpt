#include "./dependency.hpp"

#include <dds/deps.hpp>
#include <dds/error/human.hpp>
#include <dds/error/on_error.hpp>
#include <dds/error/result.hpp>
#include <dds/util/json_walk.hpp>
#include <dds/util/parse_enum.hpp>
#include <dds/util/string.hpp>

#include <boost/leaf/exception.hpp>
#include <neo/assert.hpp>
#include <neo/ranges.hpp>
#include <neo/tl.hpp>

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

project_dependency project_dependency::from_shorthand_string(std::string_view sv) {
    DDS_E_SCOPE(e_invalid_shorthand_string{std::string(sv)});
    auto split = split_view(sv, "; ");
    if (split.empty()) {
        BOOST_LEAF_THROW_EXCEPTION(e_human_message{"Invalid empty dependnency specifier"});
    }

    auto front = trim_view(split[0]);
    auto ret   = parse_dep_range_shorthand(front);

    std::optional<crs::usage_kind>        kind;
    std::optional<std::vector<dds::name>> use_names;

    auto it = split.cbegin() + 1;
    for (; it != split.cend(); ++it) {
        std::string_view part = trim_view(*it);
        if (part.starts_with("for:")) {
            auto tail = trim_view(part.substr(4));
            if (kind.has_value()) {
                BOOST_LEAF_THROW_EXCEPTION(
                    e_human_message{"Double-specified 'for' in dependency shorthand"});
            }
            kind = parse_enum_str<crs::usage_kind>(std::string(tail));
        } else if (part.starts_with("use:")) {
            auto tail = trim_view(part.substr(4));
            if (use_names.has_value()) {
                BOOST_LEAF_THROW_EXCEPTION(
                    e_human_message{"Doubule-specified 'use' in dependency shorthand"});
            }
            use_names = split_view(tail, ", ") | neo::lref
                | std::views::transform(NEO_TL(trim_view(_1)))
                | std::views::transform(NEO_TL(name{name::from_string(_1).value()}))  //
                | neo::to_vector;
        } else {
            BOOST_LEAF_THROW_EXCEPTION(
                e_human_message{neo::ufmt("Unknown part of shorthand string '{}'", part)});
        }
    }

    if (kind) {
        ret.kind = *kind;
    }
    if (use_names) {
        ret.explicit_uses = use_names;
    }
    return ret;
}