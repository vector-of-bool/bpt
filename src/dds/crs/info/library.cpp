#include "./library.hpp"

#include <dds/util/json_walk.hpp>

using namespace dds;
using namespace dds::crs;
using namespace dds::walk_utils;

namespace {

crs::intra_usage intra_usage_from_data(const json5::data& data) {
    intra_usage ret;
    using namespace semester::walk_ops;

    walk(data,
         require_mapping{"'uses' values must be JSON objects"},
         mapping{
             required_key{"lib",
                          "A 'lib' string is required",
                          require_str{"'lib' must be a value usage string"},
                          put_into{ret.lib, name_from_string{}}},
             required_key{"for",
                          "A 'for' string is required",
                          require_str{"'for' must be one of 'lib', 'app', or 'test'"},
                          put_into{ret.kind, parse_enum_str<usage_kind>}},
             if_key{"_comment", just_accept},
         });
    return ret;
}

}  // namespace

library_info library_info::from_data(const json5::data& data) {
    library_info ret;

    using namespace semester::walk_ops;

    walk(data,
         require_mapping{"Each library must be a JSON object"},
         mapping{
             required_key{"name",
                          "A library 'name' is required",
                          require_str{"Library 'name' must be a string"},
                          put_into{ret.name, name_from_string{}}},
             required_key{"path",
                          "A library 'path' is required",
                          require_str{"Library 'path' must be a string"},
                          put_into{ret.path,
                                   [](std::string s) {
                                       auto p = std::filesystem::path(s).lexically_normal();
                                       if (p.has_root_path()) {
                                           throw semester::walk_error{
                                               neo::
                                                   ufmt("Library path [{}] must be a relative path",
                                                        p.generic_string())};
                                       }
                                       if (p.begin() != p.end() && *p.begin() == "..") {
                                           throw semester::walk_error{
                                               neo::ufmt("Library path [{}] must not reach outside "
                                                         "of the distribution directory.",
                                                         p.generic_string())};
                                       }
                                       return p;
                                   }}},
             required_key{"uses",
                          "A 'uses' list is required",
                          require_array{"A library's 'uses' must be an array of usage objects"},
                          for_each{
                              put_into{std::back_inserter(ret.intra_uses), intra_usage_from_data}}},
             required_key{"depends",
                          "A 'depends' list is required",
                          require_array{"'depends' must be an array of dependency objects"},
                          for_each{put_into{std::back_inserter(ret.dependencies),
                                            dependency::from_data}}},
             if_key{"_comment", just_accept},
         });
    return ret;
}