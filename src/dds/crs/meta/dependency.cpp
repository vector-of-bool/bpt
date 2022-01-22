#include "./dependency.hpp"

#include <dds/util/json_walk.hpp>

#include <neo/overload.hpp>
#include <semver/range.hpp>

#include <sstream>

using namespace dds;
using namespace dds::crs;
using namespace dds::walk_utils;

namespace {

std::string iv_string(const pubgrub::interval_set<semver::version>::interval_type& iv) {
    if (iv.high == semver::version::max_version()) {
        return ">=" + iv.low.to_string();
    }
    if (iv.low == semver::version()) {
        return "<" + iv.high.to_string();
    }
    return iv.low.to_string() + " < " + iv.high.to_string();
}

semver::version next_major(semver::version const& v) {
    auto v2  = v;
    v2.minor = 0;
    v2.patch = 0;
    ++v2.major;
    v2.prerelease = {};
    return v2;
}

semver::version next_minor(semver::version const& v) {
    auto v2  = v;
    v2.patch = 0;
    ++v2.minor;
    v2.prerelease = {};
    return v2;
}

}  // namespace

std::string dds::crs::dependency::decl_to_string() const noexcept {
    std::stringstream strm;
    strm << name.str;
    if (acceptable_versions.num_intervals() == 1) {
        auto iv = *acceptable_versions.iter_intervals().begin();
        if (iv.high == iv.low.next_after()) {
            strm << "@" << iv.low.to_string();
        } else if (iv.high == next_major(iv.low)) {
            strm << "^" << iv.low.to_string();
        } else if (iv.high == next_minor(iv.low)) {
            strm << "~" << iv.low.to_string();
        } else if (iv.low == semver::version() && iv.high == semver::version::max_version()) {
            strm << "+" << iv.low.to_string();
        } else {
            strm << "@[" << iv_string(iv) << "]";
        }
    } else {
        strm << "@[";
        auto       iv_it = acceptable_versions.iter_intervals();
        auto       it    = iv_it.begin();
        const auto stop  = iv_it.end();
        if (it == stop) {
            // An empty version range is unsatisfiable.
            strm << "⊥";
        }
        while (it != stop) {
            strm << "(" << iv_string(*it) << ")";
            ++it;
            if (it != stop) {
                strm << " || ";
            }
        }
        strm << "]";
    }
    uses.visit(neo::overload{
        [&](implicit_uses_all) { strm << "/*"; },
        [&](explicit_uses_list const& u) {
            strm << '/' << joinstr(",", u.uses | std::views::transform(&dds::name::str));
        },
    });
    return strm.str();
}

dependency dependency::from_data(const json5::data& data) {
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