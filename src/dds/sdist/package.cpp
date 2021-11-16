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

dependency parse_dependency(const json5::data& data) {
    dependency dep;

    std::vector<name> uses;
    bool              has_use_key = false;

    using namespace semester::walk_ops;
    auto str_to_dependency
        = [](const std::string& s) { return dependency::parse_depends_string(s); };

    auto handle_mapping = mapping{
        required_key{
            "dep",
            "A 'dep' string is required",
            require_str{"'dep' key in dependency must be a string"},
            [&](const std::string& depstr) {
                auto part    = str_to_dependency(depstr);
                dep.name     = part.name;
                dep.versions = part.versions;
                return walk.accept;
            },
        },
        if_key{
            "for",
            require_str{"'for' must be a string of 'lib', 'test', or 'app'"},
            [&](const std::string& s) {
                if (s == "lib") {
                    dep.for_kind = dep_for_kind::lib;
                } else if (s == "test") {
                    dep.for_kind = dep_for_kind::test;
                } else if (s == "app") {
                    dep.for_kind = dep_for_kind::app;
                } else {
                    return walk.reject(
                        fmt::format("'for' must be one of 'lib', 'test', or 'app', but got '{}'",
                                    s));
                }
                return walk.accept;
            },
        },
        if_key{
            "use",
            require_array{"'use' must be an array of strings"},
            [&](auto&&) {
                has_use_key = true;
                return walk.pass;
            },
            for_each{require_str{"Each 'use' item must be a string"},
                     put_into(std::back_inserter(uses),
                              [](std::string s) { return *dds::name::from_string(s); })},
        },
    };

    walk(data,
         if_type<json5::data::string_type>(semester::put_into(&dep, str_to_dependency)),
         if_type<json5::data::mapping_type>(handle_mapping),
         reject_with("Each entry in 'depends' array must be a string or an object"));

    if (has_use_key) {
        dep.uses = std::move(uses);
    }
    return dep;
}

library_info parse_library(const json5::data& lib_data) {
    using namespace semester::walk_ops;
    auto str_to_usage = [](const std::string& s) { return *lm::split_usage_string(s); };
    auto set_usages   = [&](auto& opt_vec) {
        return [&](auto&& data) {
            opt_vec.emplace();
            walk(  //
                data,
                require_array{"Usage keys must be arrays of strings"},
                for_each{require_str{"Each usage item must be a string"},
                         put_into(std::back_inserter(*opt_vec), str_to_usage)});
            return walk.accept;
        };
    };

    library_info ret;
    walk(lib_data,
         require_obj{"Library entries must be objects"},
         mapping{required_key{"name",
                              "A library 'name' string is required",
                              require_str{"'name' must be a string"},
                              put_into(ret.name,
                                       [](const std::string& s) {
                                           return *dds::name::from_string(s);
                                       })},
                 if_key{"uses", set_usages(ret.uses)},
                 if_key{"test_uses", set_usages(ret.test_uses)}});
    return ret;
}

std::vector<library_info> parse_libs(const json5::data& libraries) {
    std::vector<library_info> ret;

    using namespace semester::walk_ops;

    for (const auto& [key, lib_data] : libraries.as_object()) {
        auto new_info    = parse_library(lib_data);
        new_info.relpath = key;
        ret.emplace_back(std::move(new_info));
    }
    return ret;
}

package_manifest parse_json(const json5::data& data, std::string_view fpath) {
    package_manifest ret;

    using namespace semester::walk_ops;

    static bool did_warn_test_driver = false;

    walk(data,
         require_obj{"Root of package manifest should be a JSON object"},
         mapping{
             if_key{"$schema", just_accept},
             required_key{"name",
                          "A string 'name' is required",
                          require_str{"'name' must be a string"},
                          put_into{ret.id.name,
                                   [](std::string s) {
                                       DDS_E_SCOPE(e_pkg_name_str{s});
                                       return *dds::name::from_string(s);
                                   }}},
             required_key{"namespace",
                          "A string 'namespace' is a required ",
                          require_str{"'namespace' must be a string"},
                          put_into{ret.namespace_,
                                   [](std::string s) {
                                       DDS_E_SCOPE(e_pkg_namespace_str{s});
                                       return *dds::name::from_string(s);
                                   }}},
             required_key{"version",
                          "A 'version' string is required",
                          require_str{"'version' must be a string"},
                          put_into{ret.id.version,
                                   [](std::string s) { return semver::version::parse(s); }}},
             if_key{"depends",
                    require_array{"'depends' should be an array of strings or objects"},
                    for_each{put_into(std::back_inserter(ret.dependencies), parse_dependency)}},
             if_key{"library", put_into{ret.main_library, parse_library}},
             if_key{"libraries",
                    require_obj{"'libraries' must be a JSON object"},
                    put_into(ret.subdirectory_libraries, parse_libs)},
             if_key{"test_driver",
                    [&](auto&&) {
                        if (!did_warn_test_driver) {
                            dds_log(warn,
                                    "'test_driver' is deprecated and has no effect. Use test-mode "
                                    "dependencies instead. (In [{}])",
                                    fpath);
                        }
                        did_warn_test_driver = true;
                        return walk.accept;
                    }},
         });
    return ret;
}

}  // namespace

package_manifest package_manifest::load_from_file(const fs::path& fpath) {
    DDS_E_SCOPE(e_package_manifest_path{fpath.string()});
    auto content = slurp_file(fpath);
    return load_from_json5_str(content, fpath.string());
}

package_manifest package_manifest::load_from_json5_str(std::string_view content,
                                                       std::string_view input_name) {
    DDS_E_SCOPE(e_package_manifest_path{std::string(input_name)});
    try {
        auto data = json5::parse_data(content);
        return parse_json(data, input_name);
    } catch (const semester::walk_error& e) {
        throw_user_error<errc::invalid_pkg_manifest>(e.what());
    } catch (const json5::parse_error& err) {
        BOOST_LEAF_THROW_EXCEPTION(user_error<errc::invalid_pkg_manifest>(
                                       "Invalid package manifest JSON5 document"),
                                   err,
                                   boost::leaf::e_file_name{std::string(input_name)});
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
                          DDS_E_ARG(boost::leaf::e_file_name{cand.string()}));
        }
    }

    return boost::leaf::new_error(std::errc::no_such_file_or_directory,
                                  DDS_E_ARG(
                                      e_human_message{"Expected to find a package manifest file"}),
                                  DDS_E_ARG(e_missing_file{dirpath / "package.json5"}));
}

result<package_manifest> package_manifest::load_from_directory(path_ref dirpath) {
    BOOST_LEAF_AUTO(found, find_in_directory(dirpath));
    return load_from_file(found);
}
