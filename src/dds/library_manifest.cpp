#include "./library_manifest.hpp"

#include <libman/parse.hpp>

#include <spdlog/fmt/fmt.h>

using namespace dds;

library_manifest library_manifest::load_from_file(const fs::path& fpath) {
    auto             kvs = lm::parse_file(fpath);
    library_manifest ret;
    ret.name = fpath.parent_path().filename().string();
    lm::read(fmt::format("Reading library manifest {}", fpath.string()),
             kvs,
             lm::read_accumulate("Private-Include", ret.private_includes),
             lm::read_accumulate("Private-Define", ret.private_defines),
             lm::read_accumulate("Uses", ret.uses),
             lm::read_accumulate("Links", ret.links),
             lm::read_opt("Name", ret.name),
             lm::reject_unknown());
    return ret;
}
