#include "./manifest.hpp"

#include <dds/dym.hpp>
#include <dds/util/algo.hpp>
#include <range/v3/view/transform.hpp>

#include <libman/parse.hpp>

#include <spdlog/fmt/fmt.h>

using namespace dds;

library_manifest library_manifest::load_from_file(const fs::path& fpath) {
    auto             kvs = lm::parse_file(fpath);
    library_manifest ret;
    ret.name = fpath.parent_path().filename().string();
    std::vector<std::string> uses_strings;
    std::vector<std::string> links_strings;
    lm::read(fmt::format("Reading library manifest {}", fpath.string()),
             kvs,
             lm::read_accumulate("Uses", uses_strings),
             lm::read_accumulate("Links", links_strings),
             lm::read_required("Name", ret.name),
             lm_reject_dym{{"Uses", "Links", "Name"}});

    extend(ret.uses, ranges::views::transform(uses_strings, lm::split_usage_string));
    extend(ret.links, ranges::views::transform(links_strings, lm::split_usage_string));
    return ret;
}
