#pragma once

#include <json5/data.hpp>

#include <yaml-cpp/node/node.h>

namespace sbs {

json5::data yaml_as_json5_data(const YAML::Node&);

}  // namespace sbs