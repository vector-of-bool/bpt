#include "./parse.hpp"

#include <dds/error/on_error.hpp>
#include <dds/error/result.hpp>

#include <dds/util/fs.hpp>

#include <json5/parse_data.hpp>
#include <nlohmann/json.hpp>

using namespace dds;

result<nlohmann::json> dds::parse_json_file(path_ref fpath) noexcept {
    DDS_E_SCOPE(fpath);
    BOOST_LEAF_AUTO(content, dds::read_file(fpath));
    return parse_json_str(content);
}

result<nlohmann::json> dds::parse_json_str(std::string_view content) noexcept {
    try {
        return nlohmann::json::parse(content);
    } catch (const nlohmann::json::exception& err) {
        return new_error(e_json_parse_error{err.what()});
    }
}

result<json5::data> dds::parse_json5_file(path_ref fpath) noexcept {
    DDS_E_SCOPE(fpath);
    BOOST_LEAF_AUTO(content, dds::read_file(fpath));
    return parse_json5_str(content);
}

result<json5::data> dds::parse_json5_str(std::string_view content) noexcept {
    try {
        return json5::parse_data(content);
    } catch (const json5::parse_error& e) {
        return new_error(e_json_parse_error{e.what()});
    }
}
