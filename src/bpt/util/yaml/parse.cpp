#include "./parse.hpp"

#include "./errors.hpp"

#include <bpt/error/on_error.hpp>
#include <bpt/util/fs/io.hpp>

#include <boost/leaf/exception.hpp>
#include <yaml-cpp/exceptions.h>
#include <yaml-cpp/node/parse.h>

#include <fstream>

using namespace bpt;

YAML::Node bpt::parse_yaml_file(const std::filesystem::path& fpath) {
    BPT_E_SCOPE(e_parse_yaml_file_path{fpath});
    auto content = bpt::read_file(fpath);
    return parse_yaml_string(content);
}

YAML::Node bpt::parse_yaml_string(std::string_view sv) {
    BPT_E_SCOPE(e_parse_yaml_string{std::string(sv)});
    try {
        return YAML::Load(sv.data());
    } catch (YAML::Exception const& exc) {
        BOOST_LEAF_THROW_EXCEPTION(exc, e_yaml_parse_error{exc.what()});
    }
}