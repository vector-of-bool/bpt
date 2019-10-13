#include "./manifest.hpp"

#include <dds/lm_parse.hpp>

using namespace dds;

library_manifest library_manifest::load_from_file(const fs::path& fpath) {
    auto             kvs = lm_parse_file(fpath);
    library_manifest ret;
    for (auto& pair : kvs.items()) {
        if (pair.key() == "Private-Include") {
            ret.private_includes.emplace_back(pair.value());
        } else if (pair.key() == "Private-Defines") {
            ret.private_defines.emplace_back(pair.value());
        } else {
            throw std::runtime_error("Unknown key in " + fpath.string() + ": " + pair.key());
        }
    }
    return ret;
}