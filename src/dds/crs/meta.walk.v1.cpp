#include "./meta.hpp"

#include <dds/error/result.hpp>
#include <libman/library.hpp>
#include <neo/ufmt.hpp>
#include <semester/walk.hpp>

#include <dds/util/json_walk.hpp>

#include <string>

using namespace dds;
using namespace dds::crs;
using namespace dds::walk_utils;

namespace {

crs::intra_usage intra_usage_from_data(const json5::data& data) {
    intra_usage ret;
    using namespace semester::walk_ops;

    walk(data,
         require_mapping{"'uses' values must be JSON objects"},
         mapping{
             required_key{"lib",
                          "A 'lib' string is required",
                          require_str{"'lib' must be a value usage string"},
                          put_into{ret.lib, name_from_string{}}},
             required_key{"for",
                          "A 'for' string is required",
                          require_str{"'for' must be one of 'lib', 'app', or 'test'"},
                          put_into{ret.kind, parse_enum_str<usage_kind>}},
             if_key{"_comment", just_accept},
         });
    return ret;
}

crs::dependency dependency_from_data(const json5::data& data) {
    dependency ret;

    using namespace semester::walk_ops;
    std::vector<semver::range> ver_ranges;
    std::vector<dds::name>     uses;

    auto parse_version_range = [&](const json5::data& range) {
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
            throw(
                semester::walk_error{"'high' version must be strictly greater than 'low' version"});
        }
        return semver::range{low, high};
    };

    walk(data,
         require_mapping{"Each dependency should be a JSON object"},
         mapping{
             required_key{"name",
                          "A string 'name' is required for each dependency",
                          require_str{"Dependency 'name' must be a string"},
                          put_into{ret.name, name_from_string{}}},
             required_key{"for",
                          "A 'for' is required for each dependency",
                          require_str{"Dependency 'for' must be 'lib', 'app', or 'test'"},
                          put_into{ret.kind, parse_enum_str<usage_kind>}},
             required_key{"versions",
                          "An array 'versions' is required for each dependency",
                          require_array{"Dependency 'versions' must be an array"},
                          for_each{put_into{std::back_inserter(ver_ranges), parse_version_range}}},
             required_key{"uses",
                          "A dependency 'uses' key is required",
                          require_array{"Dependency 'uses' must be an array of usage objects"},
                          for_each{require_str{"Each 'uses' item must be a usage string"},
                                   put_into{std::back_inserter(uses), name_from_string{}}}},
             if_key{"_comment", just_accept},
         });

    if (ver_ranges.empty()) {
        throw(semester::walk_error{"A dependency's 'versions' array may not be empty"});
    }

    for (auto& ver : ver_ranges) {
        ret.acceptable_versions = ret.acceptable_versions.union_(
            pubgrub::interval_set<semver::version>{ver.low(), ver.high()});
    }

    ret.uses = explicit_uses_list{std::move(uses)};

    return ret;
}

library_meta library_from_data(const json5::data& data) {
    library_meta ret;

    using namespace semester::walk_ops;

    walk(data,
         require_mapping{"Each library must be a JSON object"},
         mapping{
             required_key{"name",
                          "A library 'name' is required",
                          require_str{"Library 'name' must be a string"},
                          put_into{ret.name, name_from_string{}}},
             required_key{"path",
                          "A library 'path' is required",
                          require_str{"Library 'path' must be a string"},
                          put_into{ret.path,
                                   [](std::string s) {
                                       auto p = std::filesystem::path(s).lexically_normal();
                                       if (p.is_absolute()) {
                                           throw semester::walk_error{
                                               neo::
                                                   ufmt("Library path [{}] must be a relative path",
                                                        p.generic_string())};
                                       }
                                       if (p.begin() != p.end() && *p.begin() == "..") {
                                           throw semester::walk_error{
                                               neo::ufmt("Library path [{}] must not reach outside "
                                                         "of the distribution directory.",
                                                         p.generic_string())};
                                       }
                                       return p;
                                   }}},
             required_key{"uses",
                          "A 'uses' list is required",
                          require_array{"A library's 'uses' must be an array of usage objects"},
                          for_each{
                              put_into{std::back_inserter(ret.intra_uses), intra_usage_from_data}}},
             required_key{"depends",
                          "A 'depends' list is required",
                          require_array{"'depends' must be an array of dependency objects"},
                          for_each{put_into{std::back_inserter(ret.dependencies),
                                            dependency_from_data}}},
             if_key{"_comment", just_accept},
         });
    return ret;
}

auto require_integer_key(std::string name) {
    using namespace semester::walk_ops;
    return [name](const json5::data& dat) {
        if (!dat.is_number()) {
            return walk.reject(neo::ufmt("'{}' must be an integer", name));
        }
        double d = dat.as_number();
        if (d != (double)(int)d) {
            return walk.reject(neo::ufmt("'{}' must be an integer", name));
        }
        return walk.pass;
    };
}

package_meta meta_from_data(const json5::data& data) {
    package_meta ret;
    using namespace semester::walk_ops;

    walk(data,
         require_mapping{"Root of CRS manifest must be a JSON object"},
         mapping{
             if_key{"$schema", just_accept},
             required_key{"name",
                          "A string 'name' is required",
                          require_str{"'name' must be a string"},
                          put_into{ret.name, name_from_string{}}},
             required_key{"version",
                          "A 'version' string is required",
                          require_str{"'version' must be a string"},
                          put_into{ret.version, version_from_string{}}},
             required_key{"meta_version",
                          "A 'meta_version' integer is required",
                          require_integer_key("meta_version"),
                          put_into{ret.meta_version, [](double d) { return int(d); }}},
             required_key{"namespace",
                          "A string 'namespace' is required",
                          require_str{"'namespace' must be a string"},
                          put_into{ret.namespace_, name_from_string{}}},

             required_key{"libraries",
                          "A 'libraries' array is required",
                          require_array{"'libraries' must be an array of library objects"},
                          for_each{put_into{std::back_inserter(ret.libraries), library_from_data}}},
             required_key{"crs_version", "A 'crs_version' number is required", just_accept},
             if_key{"extra", put_into{ret.extra}},
             if_key{"_comment", just_accept},
         });

    if (ret.libraries.empty()) {
        throw semester::walk_error{"'libraries' array must be non-empty"};
    }

    return ret;
}

}  // namespace

package_meta package_meta::from_json_data_v1(const json5::data& dat) { return meta_from_data(dat); }
