#include "./deps.hpp"

#include <dds/error/errors.hpp>
#include <dds/util/string.hpp>

#include <json5/parse_data.hpp>
#include <semester/decomp.hpp>

#include <spdlog/fmt/fmt.h>

#include <cctype>
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
    auto content = slurp_file(fpath);
    auto data    = json5::parse_data(content);

    dependency_manifest depman;
    using namespace semester::decompose_ops;
    auto res = semester::decompose(
        data,
        try_seq{
            require_type<json5::data::mapping_type>{
                "The root of a dependency manifest must be an object (mapping)"},
            mapping{
                if_key{"$schema", just_accept},
                if_key{"depends",
                       require_type<json5::data::mapping_type>{
                           "`depends` must be a mapping between package names and version ranges"},
                       mapping{[&](auto pkg_name, const json5::data& range_str_) {
                           if (!range_str_.is_string()) {
                               throw_user_error<errc::invalid_pkg_manifest>(
                                   "Issue in dependency manifest: Dependency for '{}' must be a "
                                   "range string",
                                   pkg_name);
                           }
                           try {
                               auto rng = semver::range::parse_restricted(range_str_.as_string());
                               dependency dep{std::string(pkg_name), {rng.low(), rng.high()}};
                               depman.dependencies.push_back(std::move(dep));
                           } catch (const semver::invalid_range&) {
                               throw_user_error<errc::invalid_version_range_string>(
                                   "Invalid version range string '{}' in dependency declaration "
                                   "for '{}'",
                                   range_str_.as_string(),
                                   pkg_name);
                           }
                           return semester::dc_accept;
                       }}},
                [&](auto key, auto&&) {
                    return semester::dc_reject_t{
                        fmt::format("Unknown key `{}` in dependency manifest", key)};
                }},
        });
    auto rej = std::get_if<semester::dc_reject_t>(&res);
    if (rej) {
        throw_user_error<errc::invalid_pkg_manifest>(rej->message);
    }

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