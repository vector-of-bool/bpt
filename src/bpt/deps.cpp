#include "./deps.hpp"

#include <bpt/error/nonesuch.hpp>
#include <bpt/error/on_error.hpp>
#include <bpt/util/json5/parse.hpp>
#include <bpt/util/json_walk.hpp>
#include <bpt/util/yaml/convert.hpp>
#include <bpt/util/yaml/parse.hpp>

using namespace bpt;

dependency_manifest dependency_manifest::from_file(path_ref fpath) {
    BPT_E_SCOPE(e_parse_dependency_manifest_path{fpath});
    auto data = bpt::yaml_as_json5_data(bpt::parse_yaml_file(fpath));

    dependency_manifest ret;
    using namespace bpt::walk_utils;

    key_dym_tracker dym{{"dependencies"}};

    // Parse and validate
    walk(  //
        data,
        require_mapping{"The root of a dependency manifest must be a JSON object"},
        mapping{
            dym.tracker(),
            if_key{"$schema", just_accept},
            required_key{
                "dependencies",
                "A 'dependencies' key is required",
                require_array{"'dependencies' must be an array of strings"},
                for_each{put_into{std::back_inserter(ret.dependencies),
                                  project_dependency::from_json_data}},
            },
            dym.rejecter<e_bad_deps_json_key>(),
        });

    return ret;
}
