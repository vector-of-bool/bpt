#include "./source_dir.hpp"

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

using namespace dds;

std::vector<source_file> source_directory::sources() const {
    using namespace ranges::views;
    // Strips nullopt elements and lifts the value from the results
    auto drop_nulls =                                       //
        filter([](auto&& opt) { return opt.has_value(); })  //
        | transform([](auto&& opt) { return *opt; });       //

    // Collect all source files from the directory
    return                                                                              //
        fs::recursive_directory_iterator(path)                                          //
        | filter([](auto&& entry) { return entry.is_regular_file(); })                  //
        | transform([&](auto&& entry) { return source_file::from_path(entry, path); })  //
        // source_file::from_path returns an optional. Drop nulls
        | drop_nulls  //
        | ranges::to_vector;
}
