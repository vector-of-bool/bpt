#include "./import.hpp"

#include <dds/error/errors.hpp>
#include <dds/util/log.hpp>

#include <fmt/core.h>
#include <json5/parse_data.hpp>
#include <neo/assert.hpp>
#include <semester/walk.hpp>

#include <optional>

using namespace dds;

template <typename KeyFunc, typename... Args>
struct any_key {
    KeyFunc                     _key_fn;
    semester::walk_seq<Args...> _seq;

    any_key(KeyFunc&& kf, Args&&... args)
        : _key_fn(kf)
        , _seq(NEO_FWD(args)...) {}

    template <typename Data>
    semester::walk_result operator()(std::string_view key, Data&& dat) {
        auto res = _key_fn(key);
        if (res.rejected()) {
            return res;
        }
        return _seq.invoke(NEO_FWD(dat));
    }
};

template <typename KF, typename... Args>
any_key(KF&&, Args&&...) -> any_key<KF, Args...>;

namespace {

using require_obj   = semester::require_type<json5::data::mapping_type>;
using require_array = semester::require_type<json5::data::array_type>;
using require_str   = semester::require_type<std::string>;

template <typename... Args>
[[noreturn]] void import_error(Args&&... args) {
    throw_user_error<dds::errc::invalid_catalog_json>(NEO_FWD(args)...);
}

git_remote_listing parse_git_remote(const json5::data& data) {
    git_remote_listing git;

    using namespace semester::walk_ops;

    walk(data,
         require_obj{"Git remote should be an object"},
         mapping{required_key{"url",
                              "A git 'url' string is required",
                              require_str("Git URL should be a string"),
                              put_into(git.url)},
                 required_key{"ref",
                              "A git 'ref' is required, and must be a tag or branch name",
                              require_str("Git ref should be a string"),
                              put_into(git.ref)},
                 if_key{"auto-lib",
                        require_str("'auto-lib' should be a string"),
                        put_into(git.auto_lib,
                                 [](std::string const& str) {
                                     try {
                                         return lm::split_usage_string(str);
                                     } catch (const std::runtime_error& e) {
                                         import_error("{}: {}", walk.path(), e.what());
                                     }
                                 })},
                 if_key{"transform",
                        require_array{"Expect an array of transforms"},
                        for_each{put_into(std::back_inserter(git.transforms), [](auto&& dat) {
                            try {
                                return fs_transformation::from_json(dat);
                            } catch (const semester::walk_error& e) {
                                import_error(e.what());
                            }
                        })}}});

    return git;
}

package_info
parse_pkg_json_v1(std::string_view name, semver::version version, const json5::data& data) {
    package_info ret;
    ret.ident = package_id{std::string{name}, version};

    using namespace semester::walk_ops;

    auto make_dep = [&](std::string const& str) {
        try {
            return dependency::parse_depends_string(str);
        } catch (std::runtime_error const& e) {
            import_error(std::string(walk.path()) + e.what());
        }
    };

    auto check_one_remote = [&](auto&&) {
        if (!semester::holds_alternative<std::monostate>(ret.remote)) {
            return walk.reject("Cannot specify multiple remotes for a package");
        }
        return walk.pass;
    };

    walk(data,
         mapping{if_key{"description",
                        require_str{"'description' should be a string"},
                        put_into{ret.description}},
                 if_key{"depends",
                        require_array{"'depends' must be an array of dependency strings"},
                        for_each{require_str{"Each dependency should be a string"},
                                 put_into{std::back_inserter(ret.deps), make_dep}}},
                 if_key{
                     "git",
                     check_one_remote,
                     put_into(ret.remote, parse_git_remote),
                 }});

    if (semester::holds_alternative<std::monostate>(ret.remote)) {
        import_error("{}: Package listing for {} does not have any remote information",
                     walk.path(),
                     ret.ident.to_string());
    }

    return ret;
}

std::vector<package_info> parse_json_v1(const json5::data& data) {
    std::vector<package_info> acc_pkgs;

    std::string     pkg_name;
    semver::version pkg_version;
    package_info    dummy;

    using namespace semester::walk_ops;

    auto convert_pkg_obj
        = [&](auto&& dat) { return parse_pkg_json_v1(pkg_name, pkg_version, dat); };

    auto convert_version_str = [&](std::string_view str) {
        try {
            return semver::version::parse(str);
        } catch (const semver::invalid_version& e) {
            throw_user_error<errc::invalid_catalog_json>("{}: version string '{}' is invalid: {}",
                                                         walk.path(),
                                                         pkg_name,
                                                         str,
                                                         e.what());
        }
    };

    auto import_pkg_versions
        = walk_seq{require_obj{"Package entries must be JSON objects"},
                   mapping{any_key{put_into(pkg_version, convert_version_str),
                                   require_obj{"Package+version entries must be JSON"},
                                   put_into{std::back_inserter(acc_pkgs), convert_pkg_obj}}}};

    auto import_pkgs = walk_seq{require_obj{"'packages' should be a JSON object"},
                                mapping{any_key{put_into(pkg_name), import_pkg_versions}}};

    walk(data,
         mapping{
             if_key{"version", just_accept},
             required_key{"packages", "'packages' should be an object of packages", import_pkgs},
         });

    return acc_pkgs;
}

}  // namespace

std::vector<package_info> dds::parse_packages_json(std::string_view content) {
    json5::data data;
    try {
        dds_log(trace, "Parsing packages JSON data: {}", content);
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

    try {
        if (version == 1.0) {
            dds_log(trace, "Processing JSON data as v1 data");
            return parse_json_v1(data);
        } else {
            throw_user_error<errc::invalid_catalog_json>("Unknown catalog JSON version '{}'",
                                                         version);
        }
    } catch (const semester::walk_error& e) {
        throw_user_error<errc::invalid_catalog_json>(e.what());
    }
}
