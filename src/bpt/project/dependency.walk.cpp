#include "./dependency.hpp"

#include "./error.hpp"

#include <bpt/deps.hpp>
#include <bpt/error/on_error.hpp>
#include <neo/ranges.hpp>

#include <bpt/util/json_walk.hpp>
#include <semver/range.hpp>

using namespace bpt;
using namespace bpt::walk_utils;

static auto parse_version_range = [](const json5::data& range) {
    semver::version low;
    semver::version high;
    key_dym_tracker dym;
    walk(range,
         require_mapping{"'versions' elements must be objects"},
         mapping{
             dym.tracker(),
             required_key{"low",
                          "'low' version is required",
                          require_str{"'low' version must be a string"},
                          put_into{low, version_from_string{}}},
             required_key{"high",
                          "'high' version is required",
                          require_str{"'high' version must be a string"},
                          put_into{high, version_from_string{}}},
             if_key{"_comment", just_accept},
             dym.rejecter<e_bad_bpt_yaml_key>(),
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

    bool                       got_versions_key = false;
    std::vector<bpt::name>     explicit_uses;
    bool                       got_using_key = false;
    std::vector<semver::range> ver_ranges;

    key_dym_tracker dym{{"dep", "versions", "using"}};

    walk(  //
        data,
        require_mapping{"Dependency must be a shorthand string or a JSON object"},
        mapping{
            dym.tracker(),
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
                   set_true{got_versions_key},
                   for_each{require_mapping{"Each 'versions' element must be an object"},
                            put_into(std::back_inserter(ver_ranges), parse_version_range)}},
            if_key{"using",
                   require_array{"Dependency 'using' must be an array"},
                   for_each{require_str{"Each dependency 'using' item must be a string"},
                            set_true{got_using_key},
                            put_into(std::back_inserter(explicit_uses), name_from_string{})}},
            dym.rejecter<e_bad_bpt_yaml_key>(),
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

    if (got_using_key) {
        ret.using_ = std::move(explicit_uses);
    } else {
        ret.using_.push_back(ret.dep_name);
    }

    return ret;
}