#pragma once

#include <dds/util/parse_enum.hpp>
#include <dds/util/string.hpp>

#include <json5/data.hpp>
#include <magic_enum.hpp>
#include <neo/tl.hpp>
#include <neo/ufmt.hpp>
#include <semester/walk.hpp>

#include <ranges>

namespace semver {
struct version;
}

namespace dds {
struct name;
}

namespace dds::walk_utils {

using namespace semester::walk_ops;
using semester::walk_error;

using json5_mapping   = json5::data::mapping_type;
using json5_array     = json5::data::array_type;
using require_mapping = semester::require_type<json5_mapping>;
using require_array   = semester::require_type<json5_array>;
using require_str     = semester::require_type<std::string>;

struct name_from_string {
    dds::name operator()(std::string s);
};

struct version_from_string {
    semver::version operator()(std::string s);
};

}  // namespace dds::walk_utils
