#include "./deps.hpp"

#include <dds/error/nonesuch.hpp>
#include <dds/util/json5/parse.hpp>
#include <dds/util/json_walk.hpp>

using namespace dds;

dependency_manifest dependency_manifest::from_file(path_ref fpath) {
    auto data = dds::parse_json5_file(fpath);

    dependency_manifest ret;
    using namespace dds::walk_utils;

    key_dym_tracker dym{{"dependencies"}};

    // Parse and validate
    walk(  //
        data,
        require_mapping{"The root of a dependency manifest must be a JSON object"},
        mapping{
            dym.tracker(),
            required_key{
                "dependencies",
                "A 'dependencies' key is required",
                require_array{"'dependencies' must be an array of strings"},
                for_each{put_into{std::back_inserter(ret.dependencies),
                                  project_dependency::from_json_data}},
            },
            dym.rejecter<e_nonesuch>(),
        });

    return ret;
}
