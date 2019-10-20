#include "./package_manifest.hpp"

#include <libman/parse.hpp>

#include <spdlog/fmt/fmt.h>

using namespace dds;

package_manifest package_manifest::load_from_file(const fs::path& fpath) {
    auto        kvs = lm::parse_file(fpath);
    package_manifest ret;
    lm::read(fmt::format("Reading package manifest '{}'", fpath.string()),
             kvs,
             lm::read_opt("Name", ret.name),
             lm::read_opt("Version", ret.version),
             lm::reject_unknown());
    return ret;
}
