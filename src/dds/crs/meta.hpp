#pragma once

#include <dds/pkg/name.hpp>

#include <json5/data.hpp>
#include <libman/library.hpp>
#include <pubgrub/interval.hpp>
#include <semver/range.hpp>
#include <semver/version.hpp>

#include <filesystem>

namespace dds::crs {

struct e_invalid_meta_data {
    std::string value;
};

using version_range_set = pubgrub::interval_set<semver::version>;

enum class usage_kind {
    lib,
    test,
    app,
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

struct usage {
    lm::usage  lib;
    usage_kind kind;
};

struct library_meta {
    dds::name             name;
    std::filesystem::path path;
    std::vector<usage>    uses;
};

struct dependency {
    dds::name         name;
    version_range_set acceptable_versions;
    usage_kind        kind;
};

struct package_meta {
    dds::name                 name;
    dds::name                 namespace_;
    semver::version           version;
    int                       meta_version;
    std::vector<dependency>   dependencies;
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
