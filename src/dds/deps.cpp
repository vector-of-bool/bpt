#include "./deps.hpp"

#include <dds/dym.hpp>
#include <dds/error/errors.hpp>
#include <dds/repo/repo.hpp>
#include <dds/source/dist.hpp>
#include <dds/usage_reqs.hpp>
#include <dds/util/string.hpp>
#include <libman/index.hpp>
#include <libman/parse.hpp>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include <cctype>
#include <map>
#include <set>
#include <sstream>

using namespace dds;

dependency dependency::parse_depends_string(std::string_view str) {
    const auto str_begin = str.data();
    auto       str_iter  = str_begin;
    const auto str_end   = str_iter + str.size();

    while (str_iter != str_end && !std::isspace(*str_iter)) {
        ++str_iter;
    }

    auto name        = trim_view(std::string_view(str_begin, str_iter - str_begin));
    auto version_str = trim_view(std::string_view(str_iter, str_end - str_iter));

    try {
        auto rng = semver::range::parse_restricted(version_str);
        return dependency{std::string(name), {rng.low(), rng.high()}};
    } catch (const semver::invalid_range&) {
        throw_user_error<errc::invalid_version_range_string>(
            "Invalid version range string '{}' in dependency declaration '{}'", version_str, str);
    }
}

dependency_manifest dependency_manifest::from_file(path_ref fpath) {
    auto                kvs = lm::parse_file(fpath);
    dependency_manifest ret;
    lm::read(
        fmt::format("Reading dependencies from '{}'", fpath.string()),
        kvs,
        [&](auto, auto key, auto val) {
            if (key == "Depends") {
                ret.dependencies.push_back(dependency::parse_depends_string(val));
                return true;
            }
            return false;
        },
        lm_reject_dym{{"Depends"}});
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

std::string dependency::to_string() const noexcept {
    std::stringstream strm;
    strm << name << "@";
    if (versions.num_intervals() == 1) {
        auto iv = *versions.iter_intervals().begin();
        if (iv.high == iv.low.next_after()) {
            strm << iv.low.to_string();
            return strm.str();
        }
        if (iv.low == semver::version() && iv.high == semver::version::max_version()) {
            return name;
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