#include "./package_manifest.hpp"

#include <dds/util/string.hpp>
#include <libman/parse.hpp>

#include <range/v3/view/split.hpp>
#include <range/v3/view/split_when.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/fmt/fmt.h>

using namespace dds;

package_manifest package_manifest::load_from_file(const fs::path& fpath) {
    auto                     kvs = lm::parse_file(fpath);
    package_manifest         ret;
    std::string              version_str;
    std::vector<std::string> depends_strs;
    lm::read(fmt::format("Reading package manifest '{}'", fpath.string()),
             kvs,
             lm::read_required("Name", ret.name),
             lm::read_required("Version", version_str),
             lm::read_accumulate("Depends", depends_strs),
             lm::reject_unknown());

    if (ret.name.empty()) {
        throw std::runtime_error(
            fmt::format("'Name' field in [{}] may not be an empty string", fpath.string()));
    }
    if (version_str.empty()) {
        throw std::runtime_error(
            fmt::format("'Version' field in [{}] may not be an empty string", fpath.string()));
    }

    ret.version = semver::version::parse(version_str);

    ret.dependencies = depends_strs                                   //
        | ranges::views::transform(dependency::parse_depends_string)  //
        | ranges::to_vector;

    return ret;
}
