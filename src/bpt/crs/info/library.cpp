#include "./library.hpp"

#include <bpt/util/json_walk.hpp>

using namespace dds;
using namespace dds::crs;
using namespace dds::walk_utils;

library_info library_info::from_data(const json5::data& data) {
    library_info ret;

    using namespace semester::walk_ops;

    walk(data,
         require_mapping{"Each library must be a JSON object"},
         mapping{
             required_key{"name",
                          "A library 'name' string is required",
                          require_str{"Library 'name' must be a string"},
                          put_into{ret.name, name_from_string{}}},
             required_key{"path",
                          "A library 'path' string is required",
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
             required_key{"using",
                          "A 'using' array is required",
                          require_array{"A library's 'using' must be an array of usage objects"},
                          for_each{
                              put_into{std::back_inserter(ret.intra_using), name_from_string{}}}},
             required_key{"test-using",
                          "A 'test-using' array is required",
                          require_array{
                              "A library's 'test-using' must be an array of usage objects"},
                          for_each{put_into{std::back_inserter(ret.intra_test_using),
                                            name_from_string{}}}},
             required_key{"dependencies",
                          "A 'dependencies' array is required",
                          require_array{"'dependencies' must be an array of dependency objects"},
                          for_each{put_into{std::back_inserter(ret.dependencies),
                                            dependency::from_data}}},
             required_key{"test-dependencies",
                          "A 'test-dependencies' array is required",
                          require_array{
                              "'test-dependencies' must be an array of dependency objects"},
                          for_each{put_into{std::back_inserter(ret.test_dependencies),
                                            dependency::from_data}}},
             if_key{"_comment", just_accept},
         });
    return ret;
}