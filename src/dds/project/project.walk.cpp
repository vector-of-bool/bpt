#include "./project.hpp"

#include <dds/error/on_error.hpp>
#include <dds/error/result.hpp>
#include <dds/util/json_walk.hpp>

#include <magic_enum.hpp>
#include <neo/ufmt.hpp>
#include <semester/walk.hpp>

using namespace dds;
using namespace dds::walk_utils;

namespace {

auto parse_root_lib = [](json5::data::mapping_type data) {
    if (data.find("path") != data.end()) {
        throw walk_error{"Project's main 'lib' cannot have a 'path' property"};
    }
    data.emplace("path", ".");
    return project_library::from_json_data(std::move(data));
};

}  // namespace

project_manifest project_manifest::from_json_data(const json5::data& data) {
    DDS_E_SCOPE(e_parse_project_manifest_data{data});
    project_manifest ret;

    walk(data,
         require_mapping{"Project manifest root must be a mapping (JSON object)"},
         mapping{
             required_key{"name",
                          "A project 'name' is required",
                          require_str{"Project 'name' must be a string"},
                          put_into{ret.name, name_from_string{}}},
             required_key{"version",
                          "A project 'version' is required",
                          require_str{"Project 'version' must be a string"},
                          put_into{ret.version, version_from_string{}}},
             if_key{"namespace",
                    require_str{"Project 'namespace' must be a string"},
                    put_into{ret.namespace_, name_from_string{}}},
             if_key{"depends",
                    require_array{"Project 'depends' should be an array"},
                    for_each{put_into{std::back_inserter(ret.root_dependencies),
                                      project_dependency::from_json_data}}},
             if_key{"lib",
                    require_mapping{"Project 'lib' must be a mapping (JSON object)"},
                    put_into(std::back_inserter(ret.libraries), parse_root_lib)},
             if_key{"libs",
                    for_each{put_into(std::back_inserter(ret.libraries),
                                      project_library::from_json_data)}},
             if_key{"$schema", just_accept},
         });

    return ret;
}
