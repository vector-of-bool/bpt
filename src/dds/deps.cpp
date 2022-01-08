#include "./deps.hpp"

#include <dds/util/json5/parse.hpp>
#include <dds/util/json_walk.hpp>

using namespace dds;

dependency_manifest dependency_manifest::from_file(path_ref fpath) {
    auto data = dds::parse_json5_file(fpath);

    dependency_manifest ret;
    using namespace dds::walk_utils;

    // Parse and validate
    walk(  //
        data,
        require_mapping{"The root of a dependency manifest must be a JSON object"},
        mapping{
            required_key{
                "depends",
                "A 'depends' key is required",
                require_array{"'depends' must be an array of strings"},
                for_each{put_into{std::back_inserter(ret.dependencies),
                                  project_dependency::from_json_data}},
            },
        });

    return ret;
}
