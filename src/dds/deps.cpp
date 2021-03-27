#include "./deps.hpp"

#include <dds/error/errors.hpp>
#include <dds/error/on_error.hpp>
#include <dds/error/result.hpp>
#include <dds/util/log.hpp>
#include <dds/util/string.hpp>

#include <boost/leaf/exception.hpp>
#include <ctre.hpp>
#include <json5/parse_data.hpp>
#include <semester/walk.hpp>

#include <fmt/core.h>

#include <cctype>
#include <sstream>

using namespace dds;

dependency dependency::parse_depends_string(std::string_view str) {
    DDS_E_SCOPE(e_dependency_string{std::string(str)});
    auto sep_pos = str.find_first_of("=@^~+");
    if (sep_pos == str.npos) {
        throw_user_error<errc::invalid_version_range_string>("Invalid dependency string '{}'", str);
    }

    auto name = *dds::name::from_string(str.substr(0, sep_pos));

    if (str[sep_pos] != '@') {
        static bool did_warn = false;
        if (!did_warn) {
            dds_log(warn,
                    "Dependency version ranges are deprecated. All are treated as "
                    "same-major-version. (Parsing dependency '{}')",
                    str);
        }
        did_warn = true;
    }

    auto range_str = "^" + std::string(str.substr(sep_pos + 1));
    try {
        auto rng = semver::range::parse_restricted(range_str);
        return dependency{name, {rng.low(), rng.high()}};
    } catch (const semver::invalid_range&) {
        throw_user_error<errc::invalid_version_range_string>(
            "Invalid version range string '{}' in dependency string '{}'", range_str, str);
    }
}

dependency_manifest dependency_manifest::from_file(path_ref fpath) {
    auto content = slurp_file(fpath);
    auto data    = json5::parse_data(content);

    dependency_manifest depman;
    using namespace semester::walk_ops;
    // Helpers
    using require_array  = require_type<json5::data::array_type>;
    using require_string = require_type<std::string>;
    auto str_as_dep = [](const std::string& str) { return dependency::parse_depends_string(str); };
    auto append_dep = [&](auto& deps) { return put_into(std::back_inserter(deps), str_as_dep); };

    // Parse and validate
    auto res = walk.try_walk(  //
        data,
        require_type<json5::data::mapping_type>{
            "The root of a dependency manifest must be a JSON object"},
        mapping{
            required_key{
                "depends",
                "A 'depends' key is required",
                require_array{"'depends' must be an array of strings"},
                for_each{require_string{"Each dependency should be a string"},
                         append_dep(depman.dependencies)},
            },
            if_key{
                "test_depends",
                require_array{"'test_depends' must be an array of strings"},
                for_each{require_string{"Each dependency should be a string"},
                         append_dep(depman.test_dependencies)},
            },
        });

    res.throw_if_rejected<user_error<errc::invalid_pkg_manifest>>();

    return depman;
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

std::string dependency::to_string() const noexcept {
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
