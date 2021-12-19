#pragma once

#include "./dependency.hpp"

#include <json5/data.hpp>

#include <filesystem>

namespace dds::crs {

struct e_invalid_meta_data {
    std::string value;
};

struct e_given_meta_json_str {
    std::string value;
};

struct e_given_meta_json_input_name {
    std::string value;
};

struct e_given_meta_json_data {
    json5::data value;
};

struct e_invalid_usage_kind {
    std::string value;
};

struct intra_usage {
    lm::usage  lib;
    usage_kind kind;
};

struct library_meta {
    dds::name                name;
    std::filesystem::path    path;
    std::vector<intra_usage> intra_uses;
    std::vector<dependency>  dependencies;
};

struct package_meta {
    dds::name                 name;
    dds::name                 namespace_;
    semver::version           version;
    int                       meta_version;
    std::vector<library_meta> libraries;
    json5::data               extra;

    static package_meta from_json_data_v1(const json5::data&);
    static package_meta from_json_data(const json5::data&, std::string_view input_name);
    static package_meta from_json_str(std::string_view json, std::string_view input_name);
    static package_meta from_json_str(std::string_view json);

    std::string to_json(int indent) const noexcept;
    std::string to_json() const noexcept { return to_json(0); }
};

}  // namespace dds::crs
