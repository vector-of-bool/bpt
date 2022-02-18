#include "./library.hpp"

#include "./error.hpp"

#include <dds/util/json_walk.hpp>

using namespace dds;
using namespace dds::walk_utils;

project_library project_library::from_json_data(const json5::data& data) {
    project_library ret;

    std::vector<crs::intra_usage> intra_uses;
    bool                          got_use_key = false;

    key_dym_tracker dym{{"name", "path", "using", "dependencies"}};

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
                         "Each library's top-level 'using' item must be a string or a JSON object"},
                     [&](const json5::data& uses) {
                         crs::intra_usage r;
                         key_dym_tracker  dym2{{"lib", "for"}};
                         walk(
                             uses,
                             mapping{
                                 dym2.tracker(),
                                 required_key{
                                     "lib",
                                     "A library top-level 'using' object requires a 'lib' property",
                                     require_str{
                                         "Library 'using' item 'lib' property must be a string"},
                                     put_into(r.lib, name_from_string{})},
                                 required_key{
                                     "for",
                                     "A library top-level 'using' object requires a 'for' property",
                                     require_str{"Library 'using' item 'for' property must be a "
                                                 "usage-kind string"},
                                     put_into{r.kind, parse_enum_str<crs::usage_kind>}},
                                 dym2.rejecter<e_bad_pkg_yaml_key>(),
                             });
                         intra_uses.push_back(r);
                         return walk.accept;
                     }}},
             if_key{"dependencies",
                    require_array{"Library 'dependencies' must be an array of dependencies"},
                    for_each{put_into(std::back_inserter(ret.lib_dependencies),
                                      project_dependency::from_json_data)}},
             dym.rejecter<e_bad_pkg_yaml_key>(),
         });

    if (got_use_key) {
        ret.intra_uses = std::move(intra_uses);
    }
    return ret;
}