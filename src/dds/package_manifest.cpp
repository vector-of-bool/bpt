#include "./package_manifest.hpp"

#include <libman/parse.hpp>

#include <spdlog/fmt/fmt.h>

using namespace dds;

package_manifest package_manifest::load_from_file(const fs::path& fpath) {
    auto             kvs = lm::parse_file(fpath);
    package_manifest ret;
    for (auto& pair : kvs.items()) {
        if (pair.key() == "Name") {
            ret.name = pair.value();
        } else {
            throw std::runtime_error(
                fmt::format("Unknown key in file '{}': {}", fpath.string(), pair.key()));
        }
    }
    return ret;
}
