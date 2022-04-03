#include "./package.hpp"

#include <bpt/error/doc_ref.hpp>
#include <bpt/error/on_error.hpp>
#include <bpt/error/result.hpp>
#include <bpt/error/try_catch.hpp>
#include <bpt/util/algo.hpp>
#include <bpt/util/json5/convert.hpp>
#include <bpt/util/json5/parse.hpp>

#include <boost/leaf/on_error.hpp>
#include <json5/data.hpp>
#include <magic_enum.hpp>
#include <neo/assert.hpp>
#include <neo/overload.hpp>
#include <neo/ranges.hpp>
#include <neo/tl.hpp>
#include <neo/ufmt.hpp>
#include <nlohmann/json.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/view/concat.hpp>
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
        auto crs_version = data.as_object().find("schema-version");
        if (crs_version == data.as_object().cend()) {
            throw(semester::walk_error{"A 'schema-version' integer is required"});
        }
        if (!crs_version->second.is_number() || crs_version->second.as_number() != 1) {
            throw(semester::walk_error{"Only 'schema-version' == 1 is supported"});
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
            e_invalid_meta_data{neo::ufmt("Invalid semantic version string '{}'", e.string())},
            BPT_ERR_REF("invalid-version-string"));
    }
    dds_leaf_catch(e_name_str invalid_name, invalid_name_reason why)->noreturn_t {
        current_error().load(e_invalid_meta_data{neo::ufmt("Invalid name string '{}': {}",
                                                           invalid_name.value,
                                                           invalid_name_reason_str(why))});
        throw;
    };
}

std::string package_info::to_json(int indent) const noexcept {
    using json              = nlohmann::ordered_json;
    json ret_libs           = json::array();
    auto names_as_str_array = [](neo::ranges::range_of<dds::name> auto&& names) {
        auto arr = json::array();
        extend(arr, names | std::views::transform(&dds::name::str));
        return arr;
    };
    auto deps_as_json_array = [this](neo::ranges::range_of<crs::dependency> auto&& deps) {
        json ret = json::array();
        for (auto&& dep : deps) {
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
                    neo_assert(invariant,
                               false,
                               "We attempted to serialize (to_json) a CRS metadata object that "
                               "contains an implicit dependency library uses list. This should "
                               "never occur.",
                               *this);
                },
            });
            ret.push_back(json::object({
                {"name", dep.name.str},
                {"versions", versions},
                {"using", std::move(uses)},
            }));
        }
        return ret;
    };

    for (auto&& lib : this->libraries) {
        ret_libs.push_back(json::object({
            {"name", lib.name.str},
            {"path", lib.path.generic_string()},
            {"using", std::move(names_as_str_array(lib.intra_using))},
            {"test-using", std::move(names_as_str_array(lib.intra_test_using))},
            {"dependencies", deps_as_json_array(lib.dependencies)},
            {"test-dependencies", deps_as_json_array(lib.test_dependencies)},
        }));
    }
    json data = json::object({
        {"name", id.name.str},
        {"version", id.version.to_string()},
        {"pkg-version", id.revision},
        {"extra", json5_as_nlohmann_json(extra)},
        {"meta", json5_as_nlohmann_json(meta)},
        {"libraries", std::move(ret_libs)},
        {"schema-version", 1},
    });

    return indent ? data.dump(indent) : data.dump();
}

void package_info::throw_if_invalid() const {
    for (auto& lib : libraries) {
        auto all_using = ranges::views::concat(lib.intra_using, lib.intra_test_using);
        std::ranges::for_each(all_using, [&](auto name) {
            if (!ranges::contains(libraries, name, &library_info::name)) {
                BOOST_LEAF_THROW_EXCEPTION(e_invalid_meta_data{
                    neo::ufmt("Library '{}' uses non-existent sibling library '{}'",
                              lib.name.str,
                              name.str)});
            }
        });
    }
}
