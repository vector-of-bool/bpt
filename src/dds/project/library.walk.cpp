#include "./library.hpp"

#include <dds/util/json_walk.hpp>

using namespace dds;
using namespace dds::walk_utils;

project_library project_library::from_json_data(const json5::data& data) {
    project_library ret;

    std::vector<crs::intra_usage> intra_uses;
    bool                          got_use_key = false;

    walk(data,
         require_mapping{"Library entries must be a mapping (JSON object)"},
         mapping{
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
                 "uses",
                 require_array{"Library 'uses' key must be an array of strings"},
                 for_each{
                     [&](auto&&) {
                         got_use_key = true;
                         return walk.pass;
                     },
                     if_type<std::string>([&](std::string s) {
                         intra_uses.push_back(crs::intra_usage{dds::name{s}, crs::usage_kind::lib});
                         return walk.accept;
                     }),
                     require_mapping{
                         "Each library's top-level 'uses' item must be a string or a JSON object"},
                     [&](const json5::data& uses) {
                         crs::intra_usage r;
                         walk(uses,
                              mapping{
                                  required_key{
                                      "lib",
                                      "A library top-level 'uses' object requires a 'lib' property",
                                      require_str{
                                          "Library 'uses' item 'lib' property must be a string"},
                                      put_into(r.lib, name_from_string{})},
                                  required_key{
                                      "for",
                                      "A library top-level 'uses' object requires a 'for' property",
                                      require_str{"Library 'uses' item 'for' property must be a "
                                                  "usage-kind string"},
                                      put_into{r.kind, parse_enum_str<crs::usage_kind>}}});
                         intra_uses.push_back(r);
                         return walk.accept;
                     }}},
             if_key{"depends",
                    require_array{"Library 'depends' must be an array of dependencies"},
                    for_each{put_into(std::back_inserter(ret.lib_dependencies),
                                      project_dependency::from_json_data)}},
         });

    if (got_use_key) {
        ret.intra_uses = std::move(intra_uses);
    }
    return ret;
}