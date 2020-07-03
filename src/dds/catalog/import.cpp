#include "./import.hpp"

#include <dds/error/errors.hpp>

#include <json5/parse_data.hpp>
#include <neo/assert.hpp>
#include <semester/decomp.hpp>
#include <spdlog/fmt/fmt.h>

#include <optional>

using namespace dds;

template <typename... Args>
struct any_key {
    semester::try_seq<Args...> _seq;
    std::string_view&          _key;

    any_key(std::string_view& key_var, Args&&... args)
        : _seq(NEO_FWD(args)...)
        , _key{key_var} {}

    template <typename Data>
    semester::dc_result_t operator()(std::string_view key, Data&& dat) const {
        _key = key;
        return _seq.invoke(dat);
    }
};

template <typename... Args>
any_key(std::string_view, Args&&...) -> any_key<Args&&...>;

namespace {

semester::dc_result_t reject(std::string s) { return semester::dc_reject_t{s}; }
semester::dc_result_t pass   = semester::dc_pass;
semester::dc_result_t accept = semester::dc_accept;
using require_obj            = semester::require_type<json5::data::mapping_type>;

auto reject_unknown_key(std::string_view path) {
    return [path = std::string(path)](auto key, auto&&) {  //
        return reject(fmt::format("{}: unknown key '{}'", path, key));
    };
}

std::vector<dependency> parse_deps_json_v1(const json5::data& deps, std::string_view path) {
    std::vector<dependency> acc_deps;
    std::string_view        dep_name;
    std::string_view        dep_version_range_str;

    using namespace semester::decompose_ops;
    auto result = semester::decompose(  //
        deps,
        mapping{any_key{
            dep_name,
            [&](auto&& range_str) {
                if (!range_str.is_string()) {
                    throw_user_error<
                        errc::invalid_catalog_json>("{}/{} should be a string version range",
                                                    path,
                                                    dep_name);
                }
                try {
                    auto rng = semver::range::parse_restricted(range_str.as_string());
                    acc_deps.push_back(dependency{std::string{dep_name}, {rng.low(), rng.high()}});
                    return accept;
                } catch (const semver::invalid_range&) {
                    throw_user_error<errc::invalid_version_range_string>(
                        "Invalid version range string '{}' at {}/{}",
                        range_str.as_string(),
                        path,
                        dep_name);
                }
            },
        }});

    neo_assert(invariant,
               std::holds_alternative<semester::dc_accept_t>(result),
               "Parsing dependency object did not accept??");

    return acc_deps;
}

package_info parse_pkg_json_v1(std::string_view   name,
                               semver::version    version,
                               std::string_view   path,
                               const json5::data& pkg) {
    using namespace semester::decompose_ops;
    package_info ret;
    ret.ident = package_id{std::string{name}, version};

    auto result = semester::decompose(  //
        pkg,
        mapping{if_key{"description",
                       require_type<std::string>{
                           fmt::format("{}/description should be a string", path)},
                       put_into{ret.description}},
                if_key{"depends",
                       require_obj{fmt::format("{}/depends must be a JSON object", path)},
                       [&](auto&& dep_obj) {
                           ret.deps = parse_deps_json_v1(dep_obj, fmt::format("{}/depends", path));
                           return accept;
                       }},
                if_key{
                    "git",
                    require_obj{fmt::format("{}/git must be a JSON object", path)},
                    [&](auto&& git_obj) {
                        git_remote_listing git_remote;

                        auto r = semester::decompose(
                            git_obj,
                            mapping{
                                if_key{"url", put_into{git_remote.url}},
                                if_key{"ref", put_into{git_remote.ref}},
                                if_key{"auto-lib",
                                       require_type<std::string>{
                                           fmt::format("{}/git/auto-lib must be a string", path)},
                                       [&](auto&& al) {
                                           git_remote.auto_lib
                                               = lm::split_usage_string(al.as_string());
                                           return accept;
                                       }},
                                reject_unknown_key(std::string(path) + "/git"),
                            });

                        if (git_remote.url.empty() || git_remote.ref.empty()) {
                            throw_user_error<errc::invalid_catalog_json>(
                                "{}/git requires both 'url' and 'ref' non-empty string properties",
                                path);
                        }

                        ret.remote = git_remote;
                        return r;
                    },
                },
                reject_unknown_key(path)});

    if (std::holds_alternative<std::monostate>(ret.remote)) {
        throw_user_error<
            errc::invalid_catalog_json>("{}: Requires a remote listing (e.g. a 'git' proprety).",
                                        path);
    }
    auto rej = std::get_if<semester::dc_reject_t>(&result);
    if (rej) {
        throw_user_error<errc::invalid_catalog_json>("{}: {}", path, rej->message);
    }
    return ret;
}

std::vector<package_info> parse_json_v1(const json5::data& data) {
    using namespace semester::decompose_ops;
    auto packages_it = data.as_object().find("packages");
    if (packages_it == data.as_object().end() || !packages_it->second.is_object()) {
        throw_user_error<errc::invalid_catalog_json>(
            "Root JSON object requires a 'packages' property");
    }

    std::vector<package_info> acc_pkgs;

    std::string_view pkg_name;
    std::string_view pkg_version_str;

    auto result = semester::decompose(
        data,
        mapping{
            // Ignore the "version" key at this level
            if_key{"version", just_accept},
            if_key{
                "packages",
                mapping{any_key{
                    pkg_name,
                    [&](auto&& entry) {
                        if (!entry.is_object()) {
                            return reject(
                                fmt::format("/packages/{} must be a JSON object", pkg_name));
                        }
                        return pass;
                    },
                    mapping{any_key{
                        pkg_version_str,
                        [&](auto&& pkg_def) {
                            semver::version version;
                            try {
                                version = semver::version::parse(pkg_version_str);
                            } catch (const semver::invalid_version& e) {
                                throw_user_error<errc::invalid_catalog_json>(
                                    "/packages/{} version string '{}' is invalid: {}",
                                    pkg_name,
                                    pkg_version_str,
                                    e.what());
                            }
                            if (!pkg_def.is_object()) {
                                return reject(fmt::format("/packages/{}/{} must be a JSON object"));
                            }
                            auto pkg = parse_pkg_json_v1(pkg_name,
                                                         version,
                                                         fmt::format("/packages/{}/{}",
                                                                     pkg_name,
                                                                     pkg_version_str),
                                                         pkg_def);
                            acc_pkgs.emplace_back(std::move(pkg));
                            return accept;
                        },
                    }},
                }},
            },
            reject_unknown_key("/"),
        });

    auto rej = std::get_if<semester::dc_reject_t>(&result);
    if (rej) {
        throw_user_error<errc::invalid_catalog_json>(rej->message);
    }
    return acc_pkgs;
}

}  // namespace

std::vector<package_info> dds::parse_packages_json(std::string_view content) {
    json5::data data;
    try {
        data = json5::parse_data(content);
    } catch (const json5::parse_error& e) {
        throw_user_error<errc::invalid_catalog_json>("JSON5 syntax error: {}", e.what());
    }

    if (!data.is_object()) {
        throw_user_error<errc::invalid_catalog_json>("Root of import JSON must be a JSON object");
    }

    auto& data_obj   = data.as_object();
    auto  version_it = data_obj.find("version");
    if (version_it == data_obj.end() || !version_it->second.is_number()) {
        throw_user_error<errc::invalid_catalog_json>(
            "Root JSON import requires a 'version' property");
    }

    double version = version_it->second.as_number();

    if (version == 1.0) {
        return parse_json_v1(data);
    } else {
        throw_user_error<errc::invalid_catalog_json>("Unknown catalog JSON version '{}'", version);
    }
}
