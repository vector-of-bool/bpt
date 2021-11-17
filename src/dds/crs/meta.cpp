#include "./meta.hpp"

#include <dds/error/errors.hpp>
#include <dds/error/handle.hpp>
#include <dds/error/result.hpp>

#include <boost/leaf/handle_exception.hpp>
#include <json5/json5.hpp>
#include <json5/parse_data.hpp>
#include <magic_enum.hpp>
#include <neo/assert.hpp>
#include <neo/ranges.hpp>
#include <neo/ufmt.hpp>
#include <nlohmann/json.hpp>
#include <semester/walk.hpp>

using namespace dds;
using namespace dds::crs;

namespace {

static nlohmann::json j5_to_json(const json5::data& dat) {
    if (dat.is_null()) {
        return nullptr;
    } else if (dat.is_string()) {
        return dat.as_string();
    } else if (dat.is_number()) {
        return dat.as_number();
    } else if (dat.is_boolean()) {
        return dat.as_boolean();
    } else if (dat.is_array()) {
        auto ret = nlohmann::json::array();
        for (auto&& elem : dat.as_array()) {
            ret.push_back(j5_to_json(elem));
        }
        return ret;
    } else if (dat.is_object()) {
        auto ret = nlohmann::json::object();
        for (auto&& [key, value] : dat.as_object()) {
            ret.emplace(key, j5_to_json(value));
        }
        return ret;
    } else {
        neo::unreachable();
    }
}

using require_obj   = semester::require_type<json5::data::mapping_type>;
using require_array = semester::require_type<json5::data::array_type>;
using require_str   = semester::require_type<std::string>;

auto name_from_string       = [](std::string s) { return *dds::name::from_string(s); };
auto version_from_string    = [](std::string s) { return semver::version::parse(s); };
auto usage_kind_from_string = [](std::string s) -> usage_kind {
    auto v = magic_enum::enum_cast<usage_kind>(s);
    if (!v) {
        throw semester::walk_error{neo::ufmt("Invalid 'uses' 'for' string '{}'", s)};
    }
    return *v;
};

auto lm_usage_from_string = [](std::string s) { return *lm::split_usage_string(s); };

usage usage_from_data(const json5::data& data) {
    usage ret;

    using namespace semester::walk_ops;
    walk(data,
         require_obj{"Each 'uses' item must be a JSON object"},
         mapping{
             required_key{"for",
                          "A usage 'for' is required",
                          require_str{"Usage 'for' must be 'lib', 'app', or 'test'"},
                          put_into(ret.kind, usage_kind_from_string)},
             required_key{"lib",
                          "A 'lib' key is required in each 'uses' object",
                          require_str{"'lib' must be a usage string"},
                          put_into{ret.lib, lm_usage_from_string}},
             if_key{"_comment", just_accept},
         });
    return ret;
}

library_meta library_from_data(const json5::data& data) {
    library_meta ret;

    using namespace semester::walk_ops;

    walk(data,
         require_obj{"Each library must be a JSON object"},
         mapping{
             required_key{"name",
                          "A library 'name' is required",
                          require_str{"Library 'name' must be a string"},
                          put_into{ret.name, name_from_string}},
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
                          "A library 'uses' key is required",
                          require_array{"Library 'uses' must be an array of usage objects"},
                          for_each{put_into{std::back_inserter(ret.uses), usage_from_data}}},
             if_key{"_comment", just_accept},
         });
    return ret;
}

crs::dependency dependency_from_data(const json5::data& data) {
    dependency ret;

    using namespace semester::walk_ops;
    std::vector<semver::range> ver_ranges;

    auto parse_version_range = [&](const json5::data& range) {
        semver::version low;
        semver::version high;
        walk(range,
             require_obj{"'versions' elements must be objects"},
             mapping{
                 required_key{"low",
                              "'low' version is required",
                              require_str{"'low' version must be a string"},
                              put_into{low, version_from_string}},
                 required_key{"high",
                              "'high' version is required",
                              require_str{"'high' version must be a string"},
                              put_into{high, version_from_string}},
                 if_key{"_comment", just_accept},
             });
        if (high <= low) {
            BOOST_LEAF_THROW_EXCEPTION(
                semester::walk_error{"'high' version must be strictly greater than 'low' version"});
        }
        return semver::range{low, high};
    };

    walk(data,
         require_obj{"Each dependency should be a JSON object"},
         mapping{
             required_key{"name",
                          "A string 'name' is required for each dependency",
                          require_str{"Dependency 'name' must be a string"},
                          put_into{ret.name, name_from_string}},
             required_key{"versions",
                          "An array 'versions' is required for each dependency",
                          require_array{"Dependency 'versions' must be an array"},
                          for_each{put_into{std::back_inserter(ver_ranges), parse_version_range}}},
             if_key{"_comment", just_accept},
         });

    if (ver_ranges.empty()) {
        BOOST_LEAF_THROW_EXCEPTION(
            semester::walk_error{"A dependency's 'versions' array may not be empty"});
    }

    for (auto& ver : ver_ranges) {
        ret.acceptable_versions = ret.acceptable_versions.union_(
            pubgrub::interval_set<semver::version>{ver.low(), ver.high()});
    }

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

package_meta meta_from_data(const json5::data& data, std::string_view) {
    package_meta ret;
    using namespace semester::walk_ops;
    if (!data.is_object()) {
        BOOST_LEAF_THROW_EXCEPTION(
            semester::walk_error{"Root of CRS manifest must be a JSON object"});
    }
    auto crs_version = data.as_object().find("crs_version");
    if (crs_version == data.as_object().cend()) {
        BOOST_LEAF_THROW_EXCEPTION(semester::walk_error{"A 'crs_version' integer is required"});
    }
    if (!crs_version->second.is_number() || crs_version->second.as_number() != 1) {
        BOOST_LEAF_THROW_EXCEPTION(semester::walk_error{"Only 'crs_version' == 1 is supported"});
    }
    walk(data,
         require_obj{"Root of CRS manifest must be a JSON object"},
         mapping{
             if_key{"$schema", just_accept},
             required_key{"name",
                          "A string 'name' is required",
                          require_str{"'name' must be a string"},
                          put_into{ret.name, name_from_string}},
             required_key{"version",
                          "A 'version' string is required",
                          require_str{"'version' must be a string"},
                          put_into{ret.version, version_from_string}},
             required_key{"meta_version",
                          "A 'meta_version' integer is required",
                          require_integer_key("meta_version"),
                          put_into{ret.meta_version, [](double d) { return int(d); }}},
             required_key{"namespace",
                          "A string 'namespace' is required",
                          require_str{"'namespace' must be a string"},
                          put_into{ret.namespace_, name_from_string}},
             required_key{"depends",
                          "A 'depends' list is required",
                          require_array{"'depends' must be an array of dependency objects"},
                          for_each{put_into{std::back_inserter(ret.dependencies),
                                            dependency_from_data}}},
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

result<package_meta> package_meta::from_json_str(std::string_view json,
                                                 std::string_view input_name) noexcept {
    json5::data d;
    try {
        d = json5::parse_data(json);
    } catch (const json5::parse_error& e) {
        return new_error(e_invalid_meta_data{
            neo::ufmt("Invalid CRS manifest JSON document: {}", std::string_view(e.what()))});
    }
    return from_json_data(d, input_name);
}

result<package_meta> package_meta::from_json_data(const json5::data& data,
                                                  std::string_view   input) noexcept {
    return boost::leaf::try_catch(  //
        [&]() -> result<package_meta> { return meta_from_data(data, input); },
        [&](boost::leaf::catch_<semester::walk_error> exc) {
            return new_error(e_invalid_meta_data{exc.value().what()});
        },
        [&](boost::leaf::catch_<semver::invalid_version> exc) {
            return new_error(e_invalid_meta_data{
                neo::ufmt("Invalid semantic version string '{}'", exc.value().string())});
        },
        [&](e_name_str bad_name, invalid_name_reason why) {
            return new_error(bad_name,
                             why,
                             e_invalid_meta_data{neo::ufmt("Invalid name string '{}': {}",
                                                           bad_name.value,
                                                           invalid_name_reason_str(why))});
        },
        [&](lm::e_invalid_usage_string bad_usage) {
            return new_error(bad_usage,
                             e_invalid_meta_data{
                                 neo::ufmt("Invalid usage string '{}'", bad_usage.value)});
        });
}

result<package_meta> package_meta::from_json_str(std::string_view json) noexcept {
    return from_json_str(json, "<json>");
}

std::string package_meta::to_json(int indent) const noexcept {
    using json   = nlohmann::ordered_json;
    json depends = json::array();
    for (auto&& dep : dependencies) {
        json versions = json::array();
        for (auto&& ver : dep.acceptable_versions.iter_intervals()) {
            versions.push_back(json::object({
                {"low", ver.low.to_string()},
                {"high", ver.high.to_string()},
            }));
        }
        depends.push_back(json::object({
            {"name", dep.name.str},
            {"versions", versions},
        }));
    }
    json libraries = json::array();
    for (auto&& lib : this->libraries) {
        json uses = json::array();
        for (auto&& use : lib.uses) {
            uses.push_back(json::object({
                {"lib", neo::ufmt("{}/{}", use.lib.namespace_, use.lib.name)},
                {"kind", magic_enum::enum_name(use.kind)},
            }));
        }
        libraries.push_back(json::object({
            {"name", lib.name.str},
            {"uses", std::move(uses)},
        }));
    }
    json data = json::object({
        {"name", name.str},
        {"version", version.to_string()},
        {"meta_version", meta_version},
        {"namespace", namespace_.str},
        {"extra", j5_to_json(extra)},
        {"depends", std::move(depends)},
        {"libraries", std::move(libraries)},
        {"crs_version", 1},
    });

    return indent ? data.dump(indent) : data.dump();
}
