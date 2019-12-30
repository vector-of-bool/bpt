#include "./manifest.hpp"

#include <dds/dym.hpp>
#include <dds/error/errors.hpp>
#include <dds/util/string.hpp>
#include <libman/parse.hpp>

#include <range/v3/view/split.hpp>
#include <range/v3/view/split_when.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/fmt/fmt.h>

using namespace dds;

package_manifest package_manifest::load_from_file(const fs::path& fpath) {
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
        throw std::runtime_error(
            fmt::format("'Name' field in [{}] may not be an empty string", fpath.string()));
    }
    if (version_str.empty()) {
        throw std::runtime_error(
            fmt::format("'Version' field in [{}] may not be an empty string", fpath.string()));
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
