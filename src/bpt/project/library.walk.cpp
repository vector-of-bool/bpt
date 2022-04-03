#include "./library.hpp"

#include "./error.hpp"

#include <bpt/util/json_walk.hpp>

using namespace dds;
using namespace dds::walk_utils;

project_library project_library::from_json_data(const json5::data& data) {
    project_library ret;

    auto record_usings = [](auto key, auto& into) {
        return walk_seq{require_str{neo::ufmt("Each '{}' must be a string", key)},
                        put_into{std::back_inserter(into), name_from_string{}}};
    };

    key_dym_tracker dym{{"name", "path", "using", "test-using", "dependencies"}};

    walk(data,
         require_mapping{"Library entries must be a mapping (JSON object)"},
         mapping{
             dym.tracker(),
             required_key{"name",
                          "Library must have a 'name' string",
                          require_str{"Library 'name' must be a string"},
                          put_into(ret.name, name_from_string{})},
             required_key{"path",
                          // The main 'lib' library in a project must not have a 'path'
                          // property, but this condition is handled in the
                          // project_manifest::from_json_data function, which checks and inserts
                          // a single '.' for 'path'
                          "Library must have a 'path' string",
                          put_into(ret.relpath,
                                   [](std::string s) { return std::filesystem::path{s}; })},
             if_key{
                 "using",
                 require_array{"Library 'using' key must be an array of strings"},
                 for_each{record_usings("using", ret.intra_using)},
             },
             if_key{"test-using",
                    require_array{"Library 'test-using' key must be an array of strings"},
                    for_each{record_usings("test-using", ret.intra_test_using)}},
             if_key{"dependencies",
                    require_array{"Library 'dependencies' must be an array of dependencies"},
                    for_each{put_into(std::back_inserter(ret.lib_dependencies),
                                      project_dependency::from_json_data)}},
             if_key{"test-dependencies",
                    require_array{"Library 'test-dependencies' must be an array of dependencies"},
                    for_each{put_into(std::back_inserter(ret.test_dependencies),
                                      project_dependency::from_json_data)}},
             dym.rejecter<e_bad_pkg_yaml_key>(),
         });

    return ret;
}