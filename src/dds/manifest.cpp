#include "./manifest.hpp"

#include <libman/parse.hpp>

#include <spdlog/fmt/fmt.h>

using namespace dds;

library_manifest library_manifest::load_from_file(const fs::path& fpath) {
    auto             kvs = lm::parse_file(fpath);
    library_manifest ret;
    for (auto& pair : kvs.items()) {
        if (pair.key() == "Private-Include") {
            ret.private_includes.emplace_back(pair.value());
        } else if (pair.key() == "Private-Define") {
            ret.private_defines.emplace_back(pair.value());
        } else if (pair.key() == "Uses") {
            ret.uses.emplace_back(pair.value());
        } else if (pair.key() == "Links") {
            ret.uses.emplace_back(pair.value());
        } else {
            throw std::runtime_error(
                fmt::format("Unknown key in file '{}': {}", fpath.string(), pair.key()));
        }
    }
    return ret;
}
