#include "./deps.hpp"

#include <dds/error/errors.hpp>
#include <dds/error/on_error.hpp>
#include <dds/error/result.hpp>
#include <dds/util/json5/parse.hpp>
#include <dds/util/json_walk.hpp>
#include <dds/util/log.hpp>
#include <dds/util/string.hpp>

#include <boost/leaf/exception.hpp>
#include <ctre.hpp>
#include <semester/walk.hpp>

#include <cctype>
#include <sstream>

using namespace dds;

dependency dependency::parse_depends_string(std::string_view str) {
    DDS_E_SCOPE(e_dependency_string{std::string(str)});
    auto sep_pos = str.find_first_of("=@^~+");
    if (sep_pos == str.npos) {
        throw_user_error<errc::invalid_version_range_string>("Invalid dependency string '{}'", str);
    }

    auto name = dds::name::from_string(str.substr(0, sep_pos)).value();

    auto range_str = std::string(str.substr(sep_pos));
    if (range_str.front() == '@') {
        range_str[0] = '^';
    }
    try {
        auto rng = semver::range::parse_restricted(range_str);
        return dependency{name, {rng.low(), rng.high()}, dep_for_kind::lib};
    } catch (const semver::invalid_range&) {
        throw_user_error<errc::invalid_version_range_string>(
            "Invalid version range string '{}' in dependency string '{}'", range_str, str);
    }
}

dependency_manifest dependency_manifest::from_file(path_ref fpath) {
    auto data = dds::parse_json5_file(fpath);

    dependency_manifest ret;
    using namespace dds::walk_utils;

    // Parse and validate
    walk(  //
        data,
        require_mapping{"The root of a dependency manifest must be a JSON object"},
        mapping{
            required_key{
                "depends",
                "A 'depends' key is required",
                require_array{"'depends' must be an array of strings"},
                for_each{put_into{std::back_inserter(ret.dependencies),
                                  project_dependency::from_json_data}},
            },
        });

    return ret;
}

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

}  // namespace

std::string dependency::decl_to_string() const noexcept {
    std::stringstream strm;
    strm << name.str << "@";
    if (versions.num_intervals() == 1) {
        auto iv = *versions.iter_intervals().begin();
        if (iv.high == iv.low.next_after()) {
            strm << iv.low.to_string();
            return strm.str();
        }
        if (iv.low == semver::version() && iv.high == semver::version::max_version()) {
            return name.str;
        }
        strm << "[" << iv_string(iv) << "]";
        return strm.str();
    }

    strm << "[";
    auto       iv_it = versions.iter_intervals();
    auto       it    = iv_it.begin();
    const auto stop  = iv_it.end();
    while (it != stop) {
        strm << "(" << iv_string(*it) << ")";
        ++it;
        if (it != stop) {
            strm << " || ";
        }
    }
    strm << "]";
    return strm.str();
}

std::optional<dep_for_kind> dds::for_kind_from_str(std::string_view sv) noexcept {
    if (sv == "lib") {
        return dep_for_kind::lib;
    } else if (sv == "test") {
        return dep_for_kind::test;
    } else if (sv == "app") {
        return dep_for_kind::app;
    } else {
        return std::nullopt;
    }
}