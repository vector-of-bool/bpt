#include "./root.hpp"

#include <neo/memory.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

#include <neo/ranges.hpp>
#include <neo/tl.hpp>

using namespace dds;

namespace {

struct collector_state {
    fs::path                         base_path;
    fs::recursive_directory_iterator dir_iter;
};

}  // namespace

dds::collected_sources dds::collect_sources(path_ref dirpath) {
    using namespace ranges::views;
    auto state
        = neo::copy_shared(collector_state{dirpath, fs::recursive_directory_iterator{dirpath}});
    return state->dir_iter                                                          //
        | filter(NEO_TL(_1.is_regular_file()))                                      //
        | transform([state] NEO_CTL(source_file::from_path(_1, state->base_path)))  //
        | filter(NEO_TL(_1.has_value()))                                            //
        | transform([state] NEO_CTL(source_file{std::move(*_1)}))                   //
        ;
}

std::vector<source_file> source_root::collect_sources() const {
    using namespace ranges::views;
    // Collect all source files from the directory
    return dds::collect_sources(path) | neo::to_vector;
}
