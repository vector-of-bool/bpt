#pragma once

#include <json5/data.hpp>

#include <yaml-cpp/node/node.h>

namespace bpt {

json5::data yaml_as_json5_data(const YAML::Node&);

}  // namespace bpt