#include "./meta.hpp"

#include <dds/error/on_error.hpp>
#include <dds/error/result.hpp>
#include <dds/error/try_catch.hpp>
#include <dds/util/json5/convert.hpp>
#include <dds/util/json5/parse.hpp>

#include <boost/leaf/on_error.hpp>
#include <json5/data.hpp>
#include <magic_enum.hpp>
#include <neo/ufmt.hpp>
#include <nlohmann/json.hpp>
#include <semester/walk.hpp>

using namespace dds;
using namespace dds::crs;

package_meta package_meta::from_json_str(std::string_view json, std::string_view input_name) {
    DDS_E_SCOPE(e_given_meta_json_str{std::string(json)});
    DDS_E_SCOPE(e_given_meta_json_input_name{std::string(input_name)});
    auto data = parse_json_str(json);
    return from_json_data(nlohmann_json_as_json5(data), input_name);
}

package_meta package_meta::from_json_data(const json5::data& data, std::string_view input) {
    DDS_E_SCOPE(e_given_meta_json_input_name{std::string(input)});
    DDS_E_SCOPE(e_given_meta_json_data{data});
    return dds_leaf_try {
        if (!data.is_object()) {
            throw(semester::walk_error{"Root of CRS manifest must be a JSON object"});
        }
        auto crs_version = data.as_object().find("crs_version");
        if (crs_version == data.as_object().cend()) {
            throw(semester::walk_error{"A 'crs_version' integer is required"});
        }
        if (!crs_version->second.is_number() || crs_version->second.as_number() != 1) {
            throw(semester::walk_error{"Only 'crs_version' == 1 is supported"});
        }
        return from_json_data_v1(data);
    }
    dds_leaf_catch(lm::e_invalid_usage_string e)->noreturn_t {
        current_error().load(e_invalid_meta_data{neo::ufmt("Invalid usage string '{}'", e.value)});
        throw;
    }
    dds_leaf_catch(catch_<semester::walk_error> e)->noreturn_t {
        BOOST_LEAF_THROW_EXCEPTION(e.matched, e_invalid_meta_data{e.matched.what()});
    }
    dds_leaf_catch(const semver::invalid_version& e)->noreturn_t {
        BOOST_LEAF_THROW_EXCEPTION(
            e_invalid_meta_data{neo::ufmt("Invalid semantic version string '{}'", e.string())});
        throw;
    }
    dds_leaf_catch(e_name_str invalid_name, invalid_name_reason why)->noreturn_t {
        current_error().load(e_invalid_meta_data{neo::ufmt("Invalid name string '{}': {}",
                                                           invalid_name.value,
                                                           invalid_name_reason_str(why))});
        throw;
    };
}

package_meta package_meta::from_json_str(std::string_view json) {
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
            {"for", magic_enum::enum_name(dep.kind)},
            {"versions", versions},
        }));
    }
    json libraries = json::array();
    for (auto&& lib : this->libraries) {
        json uses = json::array();
        for (auto&& use : lib.uses) {
            uses.push_back(json::object({
                {"lib", neo::ufmt("{}/{}", use.lib.namespace_, use.lib.name)},
                {"for", magic_enum::enum_name(use.kind)},
            }));
        }
        libraries.push_back(json::object({
            {"name", lib.name.str},
            {"uses", std::move(uses)},
            {"path", lib.path.generic_string()},
        }));
    }
    json data = json::object({
        {"name", name.str},
        {"version", version.to_string()},
        {"meta_version", meta_version},
        {"namespace", namespace_.str},
        {"extra", json5_as_nlohmann_json(extra)},
        {"depends", std::move(depends)},
        {"libraries", std::move(libraries)},
        {"crs_version", 1},
    });

    return indent ? data.dump(indent) : data.dump();
}
