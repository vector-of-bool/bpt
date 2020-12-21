#include "./package.hpp"

#include <dds/dym.hpp>
#include <dds/error/errors.hpp>
#include <dds/util/log.hpp>
#include <dds/util/result.hpp>
#include <dds/util/string.hpp>

#include <boost/leaf/common.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/split_when.hpp>
#include <range/v3/view/transform.hpp>
#include <semester/walk.hpp>

#include <json5/parse_data.hpp>

using namespace dds;

namespace {

using require_obj   = semester::require_type<json5::data::mapping_type>;
using require_array = semester::require_type<json5::data::array_type>;
using require_str   = semester::require_type<std::string>;

package_manifest parse_json(const json5::data& data, std::string_view fpath) {
    package_manifest ret;

    using namespace semester::walk_ops;
    auto push_depends_obj_kv = [&](std::string key, auto&& dat) {
        dependency pending_dep;
        if (!dat.is_string()) {
            return walk.reject("Dependency object values should be strings");
        }
        try {
            auto       rng = semver::range::parse_restricted(dat.as_string());
            dependency dep{std::move(key), {rng.low(), rng.high()}};
            ret.dependencies.push_back(std::move(dep));
        } catch (const semver::invalid_range&) {
            throw_user_error<errc::invalid_version_range_string>(
                "Invalid version range string '{}' in dependency declaration for "
                "'{}'",
                dat.as_string(),
                key);
        }
        return walk.accept;
    };

    walk(data,
         require_obj{"Root of package manifest should be a JSON object"},
         mapping{
             if_key{"$schema", just_accept},
             required_key{"name",
                          "A string 'name' is required",
                          require_str{"'name' must be a string"},
                          put_into{ret.id.name}},
             required_key{"namespace",
                          "A string 'namespace' is a required ",
                          require_str{"'namespace' must be a string"},
                          put_into{ret.namespace_}},
             required_key{"version",
                          "A 'version' string is requried",
                          require_str{"'version' must be a string"},
                          put_into{ret.id.version,
                                   [](std::string s) { return semver::version::parse(s); }}},
             if_key{"depends",
                    [&](auto&& dat) {
                        if (dat.is_object()) {
                            dds_log(warn,
                                    "{}: Using a JSON object for 'depends' is deprecated. Use an "
                                    "array of strings instead.",
                                    fpath);
                            return mapping{push_depends_obj_kv}(dat);
                        } else if (dat.is_array()) {
                            return for_each{put_into{std::back_inserter(ret.dependencies),
                                                     [](const std::string& depstr) {
                                                         return dependency::parse_depends_string(
                                                             depstr);
                                                     }}}(dat);
                        } else {
                            return walk.reject(
                                "'depends' should be an array of dependency strings");
                        }
                    }},
             if_key{"test_driver",
                    require_str{"'test_driver' must be a string"},
                    put_into{ret.test_driver,
                             [](std::string const& td_str) {
                                 if (td_str == "Catch-Main") {
                                     return test_lib::catch_main;
                                 } else if (td_str == "Catch") {
                                     return test_lib::catch_;
                                 } else {
                                     auto dym = *did_you_mean(td_str, {"Catch-Main", "Catch"});
                                     throw_user_error<errc::unknown_test_driver>(
                                         "Unknown 'test_driver' '{}' (Did you mean '{}'?)",
                                         td_str,
                                         dym);
                                 }
                             }}},
         });
    return ret;
}

}  // namespace

package_manifest package_manifest::load_from_file(const fs::path& fpath) {
    auto content = slurp_file(fpath);
    return load_from_json5_str(content, fpath.string());
}

package_manifest package_manifest::load_from_json5_str(std::string_view content,
                                                       std::string_view input_name) {
    try {
        auto data = json5::parse_data(content);
        return parse_json(data, input_name);
    } catch (const semester::walk_error& e) {
        throw_user_error<errc::invalid_pkg_manifest>(e.what());
    } catch (const json5::parse_error& err) {
        BOOST_LEAF_THROW_EXCEPTION(user_error<errc::invalid_pkg_manifest>(
                                       "Invalid package manifest JSON5 document"),
                                   err,
                                   boost::leaf::e_file_name{std::string(input_name)},
                                   DDS_ERROR_MARKER("package-json5-parse-error"));
    }
}

result<fs::path> package_manifest::find_in_directory(path_ref dirpath) {
    auto cands = {
        "package.json5",
        "package.jsonc",
        "package.json",
    };
    for (auto c : cands) {
        auto            cand = dirpath / c;
        std::error_code ec;
        if (fs::is_regular_file(cand, ec)) {
            return cand;
        }
        if (ec != std::errc::no_such_file_or_directory) {
            return boost::leaf::
                new_error(ec,
                          DDS_E_ARG(e_human_message{
                              "Failed to check for package manifest in project directory"}),
                          DDS_ERROR_MARKER("failed-package-json5-scan"),
                          DDS_E_ARG(boost::leaf::e_file_name{cand.string()}));
        }
    }

    return boost::leaf::new_error(std::errc::no_such_file_or_directory,
                                  DDS_E_ARG(
                                      e_human_message{"Expected to find a package manifest file"}),
                                  DDS_E_ARG(e_missing_file{dirpath / "package.json5"}),
                                  DDS_ERROR_MARKER("no-package-json5"));
}

result<package_manifest> package_manifest::load_from_directory(path_ref dirpath) {
    BOOST_LEAF_AUTO(found, find_in_directory(dirpath));
    return load_from_file(found);
}
