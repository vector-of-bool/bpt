#include "./package.hpp"

#include <dds/error/on_error.hpp>
#include <dds/error/result.hpp>
#include <dds/error/try_catch.hpp>
#include <dds/util/algo.hpp>
#include <dds/util/json5/convert.hpp>
#include <dds/util/json5/parse.hpp>

#include <boost/leaf/on_error.hpp>
#include <json5/data.hpp>
#include <magic_enum.hpp>
#include <neo/assert.hpp>
#include <neo/overload.hpp>
#include <neo/tl.hpp>
#include <neo/ufmt.hpp>
#include <nlohmann/json.hpp>
#include <semester/walk.hpp>

using namespace dds;
using namespace dds::crs;

package_info package_info::from_json_str(std::string_view json) {
    DDS_E_SCOPE(e_given_meta_json_str{std::string(json)});
    auto data = parse_json_str(json);
    return from_json_data(nlohmann_json_as_json5(data));
}

package_info package_info::from_json_data(const json5::data& data) {
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
    }
    dds_leaf_catch(e_name_str invalid_name, invalid_name_reason why)->noreturn_t {
        current_error().load(e_invalid_meta_data{neo::ufmt("Invalid name string '{}': {}",
                                                           invalid_name.value,
                                                           invalid_name_reason_str(why))});
        throw;
    };
}

std::string package_info::to_json(int indent) const noexcept {
    using json    = nlohmann::ordered_json;
    json ret_libs = json::array();
    for (auto&& lib : this->libraries) {
        json lib_intra_uses = json::array();
        for (auto&& use : lib.intra_uses) {
            lib_intra_uses.push_back(json::object({
                {"lib", use.lib.str},
                {"for", magic_enum::enum_name(use.kind)},
            }));
        }
        json depends = json::array();
        for (auto&& dep : lib.dependencies) {
            json versions = json::array();
            for (auto&& ver : dep.acceptable_versions.iter_intervals()) {
                versions.push_back(json::object({
                    {"low", ver.low.to_string()},
                    {"high", ver.high.to_string()},
                }));
            }
            json uses = json::array();
            dep.uses.visit(neo::overload{
                [&](explicit_uses_list const& l) {
                    extend(uses, l.uses | std::views::transform(&dds::name::str));
                },
                [&](implicit_uses_all) {
                    neo_assert(
                        invariant,
                        false,
                        "We attempted to serialize (to_json) a CRS metadata object that contains "
                        "an implicit dependency library uses list. This should never occur.",
                        *this);
                },
            });
            depends.push_back(json::object({
                {"name", dep.name.str},
                {"for", magic_enum::enum_name(dep.kind)},
                {"versions", versions},
                {"uses", std::move(uses)},
            }));
        }
        ret_libs.push_back(json::object({
            {"name", lib.name.str},
            {"path", lib.path.generic_string()},
            {"uses", std::move(lib_intra_uses)},
            {"depends", std::move(depends)},
        }));
    }
    json data = json::object({
        {"name", id.name.str},
        {"version", id.version.to_string()},
        {"pkg_revision", id.pkg_revision},
        {"extra", json5_as_nlohmann_json(extra)},
        {"libraries", std::move(ret_libs)},
        {"crs_version", 1},
    });

    return indent ? data.dump(indent) : data.dump();
}

void package_info::throw_if_invalid() const {
    for (auto& lib : libraries) {
        for (auto&& uses : lib.intra_uses) {
            if (std::ranges::find(libraries, uses.lib, &library_info::name) == libraries.end()) {
                BOOST_LEAF_THROW_EXCEPTION(e_invalid_meta_data{
                    neo::ufmt("Library '{}' uses non-existent sibling library '{}'",
                              lib.name.str,
                              uses.lib.str)});
            }
        }
    }
}
