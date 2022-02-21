#include "./convert.hpp"

#include <neo/assert.hpp>
#include <nlohmann/json.hpp>

using namespace dds;

nlohmann::json dds::json5_as_nlohmann_json(const json5::data& dat) noexcept {
    if (dat.is_null()) {
        return nullptr;
    } else if (dat.is_string()) {
        return dat.as_string();
    } else if (dat.is_number()) {
        return dat.as_number();
    } else if (dat.is_boolean()) {
        return dat.as_boolean();
    } else if (dat.is_array()) {
        auto ret = nlohmann::json::array();
        for (auto&& elem : dat.as_array()) {
            ret.push_back(json5_as_nlohmann_json(elem));
        }
        return ret;
    } else if (dat.is_object()) {
        auto ret = nlohmann::json::object();
        for (auto&& [key, value] : dat.as_object()) {
            ret.emplace(key, json5_as_nlohmann_json(value));
        }
        return ret;
    } else {
        neo::unreachable();
    }
}

json5::data dds::nlohmann_json_as_json5(const nlohmann::json& data) noexcept {
    if (data.is_null()) {
        return nullptr;
    } else if (data.is_string()) {
        return std::string(data);
    } else if (data.is_number()) {
        return double(data);
    } else if (data.is_boolean()) {
        return bool(data);
    } else if (data.is_array()) {
        auto ret = json5::data::array_type{};
        for (const nlohmann::json& elem : data) {
            ret.push_back(nlohmann_json_as_json5(elem));
        }
        return json5::data(std::move(ret));
    } else if (data.is_object()) {
        auto ret = json5::data::mapping_type{};
        for (const auto& [key, value] : data.items()) {
            ret.emplace(key, nlohmann_json_as_json5(value));
        }
        return json5::data(std::move(ret));
    } else {
        neo::unreachable();
    }
}
