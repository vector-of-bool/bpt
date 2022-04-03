#include "./convert.hpp"

#include "./errors.hpp"

#include <bpt/error/on_error.hpp>

#include <boost/leaf/exception.hpp>
#include <neo/ufmt.hpp>
#include <neo/utility.hpp>
#include <yaml-cpp/node/iterator.h>
#include <yaml-cpp/yaml.h>

using namespace bpt;

template <typename T>
static T try_decode(const YAML::Node& node) {
    try {
        return node.as<T>();
    } catch (const YAML::TypedBadConversion<T>& exc) {
        BOOST_LEAF_THROW_EXCEPTION(e_yaml_invalid_spelling{node.Scalar()});
    }
}

json5::data bpt::yaml_as_json5_data(const YAML::Node& node) {
    std::string_view tag = node.Tag();
    BPT_E_SCOPE(e_yaml_tag{std::string(tag)});
    if (node.IsNull()) {
        return json5::data::null_type{};
    } else if (node.IsSequence()) {
        auto arr = json5::data::array_type{};
        for (auto& elem : node) {
            arr.push_back(yaml_as_json5_data(elem));
        }
        return arr;
    } else if (node.IsMap()) {
        auto obj = json5::data::mapping_type{};
        for (auto& pair : node) {
            obj.emplace(pair.first.as<std::string>(), yaml_as_json5_data(pair.second));
        }
        return obj;
    } else if (tag == neo::oper::any_of("!", "tag:yaml.org,2002:str")) {
        return node.as<std::string>();
    } else if (tag == "?") {
        auto spell = node.Scalar();
        if (spell == neo::oper::any_of("true", "false")) {
            return node.as<bool>();
        }
        try {
            return node.as<double>();
        } catch (const YAML::TypedBadConversion<double>&) {
            return spell;
        }
    } else if (tag == "tag:yaml.org,2002:bool") {
        return try_decode<bool>(node);
    } else if (tag == "tag:yaml.org,2002:null") {
        return nullptr;
    } else if (tag == "tag:yaml.org,2002:int") {
        return static_cast<double>(try_decode<double>(node));
    } else if (tag == "tag:yaml.org,2002:float") {
        return try_decode<double>(node);
    } else {
        BOOST_LEAF_THROW_EXCEPTION(e_yaml_unknown_tag{node.Tag()});
    }
}
