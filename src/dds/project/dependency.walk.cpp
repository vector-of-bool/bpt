#include "./dependency.hpp"

#include <dds/deps.hpp>
#include <dds/error/on_error.hpp>
#include <neo/ranges.hpp>

#include <dds/util/json_walk.hpp>

using namespace dds;
using namespace dds::walk_utils;

static auto parse_version_range = [](const json5::data& range) {
    semver::version low;
    semver::version high;
    walk(range,
         require_mapping{"'versions' elements must be objects"},
         mapping{
             required_key{"low",
                          "'low' version is required",
                          require_str{"'low' version must be a string"},
                          put_into{low, version_from_string{}}},
             required_key{"high",
                          "'high' version is required",
                          require_str{"'high' version must be a string"},
                          put_into{high, version_from_string{}}},
             if_key{"_comment", just_accept},
         });
    if (high <= low) {
        throw(semester::walk_error{"'high' version must be strictly greater than 'low' version"});
    }
    return semver::range{low, high};
};

project_dependency project_dependency::from_json_data(const json5::data& data) {
    if (data.is_string()) {
        return project_dependency::from_shorthand_string(data.as_string());
    }

    project_dependency ret;

    std::optional<crs::usage_kind> dep_kind;
    bool                           got_versions_key = false;
    std::vector<dds::name>         explicit_uses;
    bool                           got_uses_key = false;
    std::vector<semver::range>     ver_ranges;

    walk(  //
        data,
        require_mapping{"Dependency must be a shorthand string or a JSON object"},
        mapping{
            required_key{
                "dep",
                "A 'dep' key is required in a dependency object",
                require_str{"Dependency 'dep' must be a string"},
                // We'll handle this later on, since handling depends on the presence/absence of
                // 'versions'
                just_accept,
            },
            if_key{"versions",
                   require_array{"Dependency 'versions' must be an array of objects"},
                   [&](auto&&) {
                       got_versions_key = true;
                       return walk.pass;
                   },
                   for_each{require_mapping{"Each 'versions' element must be an object"},
                            put_into(std::back_inserter(ver_ranges), parse_version_range)}},
            if_key{"for",
                   require_str{"Dependency 'for' must be a string"},
                   put_into{dep_kind, parse_enum_str<crs::usage_kind>}},
            if_key{"use",
                   require_array{"Dependency 'use' must be an array"},
                   for_each{require_str{"Each dependency 'use' item must be a string"},
                            [&](auto&&) {
                                got_uses_key = true;
                                return walk.pass;
                            },
                            put_into(std::back_inserter(explicit_uses), name_from_string{})}},
        });

    for (auto& ver : ver_ranges) {
        ret.acceptable_versions = ret.acceptable_versions.union_(
            pubgrub::interval_set<semver::version>{ver.low(), ver.high()});
    }

    auto& dep_string = data.as_object().find("dep")->second.as_string();
    if (got_versions_key) {
        ret.dep_name = name_from_string{}(dep_string);
    } else {
        auto partial            = parse_dep_range_shorthand(dep_string);
        ret.dep_name            = partial.dep_name;
        ret.acceptable_versions = partial.acceptable_versions;
    }

    if (got_uses_key) {
        ret.explicit_uses = std::move(explicit_uses);
    }

    return ret;
}