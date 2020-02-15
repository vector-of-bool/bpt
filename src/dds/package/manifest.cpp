#include "./manifest.hpp"

#include <dds/dym.hpp>
#include <dds/error/errors.hpp>
#include <dds/util/string.hpp>
#include <libman/parse.hpp>

#include <range/v3/view/split.hpp>
#include <range/v3/view/split_when.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include <json5/parse_data.hpp>

using namespace dds;

package_manifest package_manifest::load_from_dds_file(const fs::path& fpath) {
    spdlog::warn(
        "Using deprecated package.dds parsing (on file {}). This will be removed soon. Migrate!",
        fpath.string());
    auto                       kvs = lm::parse_file(fpath);
    package_manifest           ret;
    std::string                version_str;
    std::vector<std::string>   depends_strs;
    std::optional<std::string> opt_test_driver;
    lm::read(fmt::format("Reading package manifest '{}'", fpath.string()),
             kvs,
             lm::read_required("Name", ret.pkg_id.name),
             lm::read_opt("Namespace", ret.namespace_),
             lm::read_required("Version", version_str),
             lm::read_accumulate("Depends", depends_strs),
             lm::read_opt("Test-Driver", opt_test_driver),
             lm_reject_dym{{"Name", "Namespace", "Version", "Depends", "Test-Driver"}});

    if (ret.pkg_id.name.empty()) {
        throw_user_error<errc::invalid_pkg_name>("'Name' field in [{}] may not be an empty string",
                                                 fpath.string());
    }
    if (version_str.empty()) {
        throw_user_error<
            errc::invalid_version_string>("'Version' field in [{}] may not be an empty string",
                                          fpath.string());
    }
    if (opt_test_driver) {
        auto& test_driver_str = *opt_test_driver;
        if (test_driver_str == "Catch-Main") {
            ret.test_driver = test_lib::catch_main;
        } else if (test_driver_str == "Catch") {
            ret.test_driver = test_lib::catch_;
        } else {
            auto dym = *did_you_mean(test_driver_str, {"Catch-Main", "Catch"});
            throw_user_error<
                errc::unknown_test_driver>("Unknown 'Test-Driver' '{}' (Did you mean '{}'?)",
                                           test_driver_str,
                                           dym);
        }
    }

    if (ret.namespace_.empty()) {
        ret.namespace_ = ret.pkg_id.name;
    }

    ret.pkg_id.version = semver::version::parse(version_str);

    ret.dependencies = depends_strs                                   //
        | ranges::views::transform(dependency::parse_depends_string)  //
        | ranges::to_vector;

    return ret;
}

package_manifest package_manifest::load_from_file(const fs::path& fpath) {
    auto content = slurp_file(fpath);
    auto data    = json5::parse_data(content);

    if (!data.is_object()) {
        throw_user_error<errc::invalid_pkg_manifest>("Root value must be an object");
    }

    const auto&      obj = data.as_object();
    package_manifest ret;

    /// Get the name
    auto it = obj.find("name");
    if (it == obj.end() || !it->second.is_string() || it->second.as_string().empty()) {
        throw_user_error<errc::invalid_pkg_name>("'name' field in [{}] must be a non-empty string",
                                                 fpath.string());
    }
    ret.namespace_ = ret.pkg_id.name = it->second.as_string();

    /// Get the version
    it = obj.find("version");
    if (it == obj.end() || !it->second.is_string()) {
        throw_user_error<
            errc::invalid_version_string>("'version' field in [{}] must be a version string",
                                          fpath.string());
    }
    auto version_str   = it->second.as_string();
    ret.pkg_id.version = semver::version::parse(version_str);

    /// Get the namespace
    it = obj.find("namespace");
    if (it != obj.end()) {
        if (!it->second.is_string() || it->second.as_string().empty()) {
            throw_user_error<errc::invalid_pkg_manifest>(
                "'namespace' attribute in [{}] must be a non-empty string", fpath.string());
        }
        ret.namespace_ = it->second.as_string();
    }

    /// Get the test driver
    it = obj.find("test_driver");
    if (it != obj.end()) {
        if (!it->second.is_string()) {
            throw_user_error<errc::invalid_pkg_manifest>(
                "'test_driver' attribute in [{}] must be a non-empty string", fpath.string());
        }
        auto& test_driver_str = it->second.as_string();
        if (test_driver_str == "Catch-Main") {
            ret.test_driver = test_lib::catch_main;
        } else if (test_driver_str == "Catch") {
            ret.test_driver = test_lib::catch_;
        } else {
            auto dym = *did_you_mean(test_driver_str, {"Catch-Main", "Catch"});
            throw_user_error<
                errc::unknown_test_driver>("Unknown 'Test-Driver' '{}' (Did you mean '{}'?)",
                                           test_driver_str,
                                           dym);
        }
    }

    /// Get the dependencies
    it = obj.find("depends");
    if (it != obj.end()) {
        if (!it->second.is_object()) {
            throw_user_error<errc::invalid_pkg_manifest>(
                "'depends' field must be an object mapping package name to version ranges");
        }

        for (const auto& [pkg_name, range_str_] : it->second.as_object()) {
            if (!range_str_.is_string()) {
                throw_user_error<
                    errc::invalid_pkg_manifest>("Dependency for '{}' must be a range string",
                                                pkg_name);
            }
            try {
                auto       rng = semver::range::parse_restricted(range_str_.as_string());
                dependency dep{pkg_name, {rng.low(), rng.high()}};
                ret.dependencies.push_back(std::move(dep));
            } catch (const semver::invalid_range&) {
                throw_user_error<errc::invalid_version_range_string>(
                    "Invalid version range string '{}' in dependency declaration for '{}'",
                    range_str_.as_string(),
                    pkg_name);
            }
        }
    }

    return ret;
}
