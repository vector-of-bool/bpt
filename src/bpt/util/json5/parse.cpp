#include "./parse.hpp"

#include "./error.hpp"

#include <bpt/error/on_error.hpp>

#include <bpt/util/fs/io.hpp>

#include <boost/leaf/exception.hpp>
#include <json5/parse_data.hpp>
#include <nlohmann/json.hpp>

using namespace bpt;

nlohmann::json bpt::parse_json_file(std::filesystem::path const& fpath) {
    BPT_E_SCOPE(fpath);
    auto content = bpt::read_file(fpath);
    return parse_json_str(content);
}

nlohmann::json bpt::parse_json_str(std::string_view content) {
    BPT_E_SCOPE(e_json_string{std::string(content)});
    try {
        return nlohmann::json::parse(content);
    } catch (const nlohmann::json::exception& err) {
        BOOST_LEAF_THROW_EXCEPTION(err, e_json_parse_error{err.what()});
    }
}

json5::data bpt::parse_json5_file(std::filesystem::path const& fpath) {
    BPT_E_SCOPE(fpath);
    auto content = bpt::read_file(fpath);
    return parse_json5_str(content);
}

json5::data bpt::parse_json5_str(std::string_view content) {
    BPT_E_SCOPE(e_json5_string{std::string(content)});
    try {
        return json5::parse_data(content);
    } catch (const json5::parse_error& err) {
        BOOST_LEAF_THROW_EXCEPTION(err, e_json_parse_error{err.what()});
    }
}
