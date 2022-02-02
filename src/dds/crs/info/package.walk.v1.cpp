#include "./package.hpp"

#include <dds/error/result.hpp>
#include <libman/library.hpp>
#include <neo/ufmt.hpp>

#include <dds/util/json_walk.hpp>

#include <string>

using namespace dds;
using namespace dds::crs;
using namespace dds::walk_utils;

namespace {

auto require_integer_key(std::string name) {
    using namespace semester::walk_ops;
    return [name](const json5::data& dat) {
        if (!dat.is_number()) {
            return walk.reject(neo::ufmt("'{}' must be an integer", name));
        }
        double d = dat.as_number();
        if (d != (double)(int)d) {
            return walk.reject(neo::ufmt("'{}' must be an integer", name));
        }
        return walk.pass;
    };
}

package_info meta_from_data(const json5::data& data) {
    package_info ret;
    using namespace semester::walk_ops;

    walk(data,
         require_mapping{"Root of CRS manifest must be a JSON object"},
         mapping{
             if_key{"$schema", just_accept},
             required_key{"name",
                          "A string 'name' is required",
                          require_str{"'name' must be a string"},
                          put_into{ret.id.name, name_from_string{}}},
             required_key{"version",
                          "A 'version' string is required",
                          require_str{"'version' must be a string"},
                          put_into{ret.id.version, version_from_string{}}},
             required_key{"pkg_revision",
                          "A 'pkg_revision' integer is required",
                          require_integer_key("pkg_revision"),
                          put_into{ret.id.pkg_revision, [](double d) { return int(d); }}},
             required_key{"libraries",
                          "A 'libraries' array is required",
                          require_array{"'libraries' must be an array of library objects"},
                          for_each{put_into{std::back_inserter(ret.libraries),
                                            library_info::from_data}}},
             required_key{"crs_version", "A 'crs_version' number is required", just_accept},
             if_key{"extra", put_into{ret.extra}},
             if_key{"_comment", just_accept},
         });

    if (ret.libraries.empty()) {
        throw semester::walk_error{"'libraries' array must be non-empty"};
    }

    return ret;
}

}  // namespace

package_info package_info::from_json_data_v1(const json5::data& dat) { return meta_from_data(dat); }
