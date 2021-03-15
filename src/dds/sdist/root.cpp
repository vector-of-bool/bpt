#include "./root.hpp"

#include <neo/ranges.hpp>
#include <neo/ref.hpp>
#include <neo/tl.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

using namespace dds;

std::vector<source_file> source_root::collect_sources() const {
    using namespace ranges::views;
    // Collect all source files from the directory
    return                                                     //
        fs::recursive_directory_iterator(path)                 //
        | neo::lref                                            //
        | filter(NEO_TL(_1.is_regular_file()))                 //
        | transform(NEO_TL(source_file::from_path(_1, path)))  //
        // source_file::from_path returns an optional. Drop nulls
        | filter(NEO_TL(_1.has_value()))                 //
        | transform(NEO_TL(source_file{NEO_MOVE(*_1)}))  //
        | neo::to_vector;
}
