#include "./parse.hpp"

#include "./errors.hpp"

#include <bpt/error/on_error.hpp>
#include <bpt/util/fs/io.hpp>

#include <boost/leaf/exception.hpp>
#include <yaml-cpp/exceptions.h>
#include <yaml-cpp/node/parse.h>

#include <fstream>

using namespace sbs;

YAML::Node sbs::parse_yaml_file(const std::filesystem::path& fpath) {
    DDS_E_SCOPE(e_parse_yaml_file_path{fpath});
    auto content = dds::read_file(fpath);
    return parse_yaml_string(content);
}

YAML::Node sbs::parse_yaml_string(std::string_view sv) {
    DDS_E_SCOPE(e_parse_yaml_string{std::string(sv)});
    try {
        return YAML::Load(sv.data());
    } catch (YAML::Exception const& exc) {
        BOOST_LEAF_THROW_EXCEPTION(exc, e_yaml_parse_error{exc.what()});
    }
}