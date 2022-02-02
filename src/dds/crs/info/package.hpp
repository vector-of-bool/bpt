#pragma once

#include "./library.hpp"
#include "./pkg_id.hpp"

#include <json5/data.hpp>
#include <magic_enum.hpp>

#include <filesystem>

namespace dds::crs {

struct e_invalid_meta_data {
    std::string value;
};

struct e_given_meta_json_str {
    std::string value;
};

struct e_pkg_json_path {
    std::filesystem::path value;
};

struct e_given_meta_json_data {
    json5::data value;
};

struct e_invalid_usage_kind {
    std::string value;
};

struct package_info {
    pkg_id                    id;
    std::vector<library_info> libraries;
    json5::data               extra;

    static package_info from_json_data_v1(const json5::data&);
    static package_info from_json_data(const json5::data&);
    static package_info from_json_str(std::string_view json);

    void throw_if_invalid() const;

    std::string to_json(int indent) const noexcept;
    std::string to_json() const noexcept { return to_json(0); }

    friend void do_repr(auto out, const package_info* self) noexcept {
        out.type("dds::crs::package_info");
        if (self) {
            out.bracket_value("id={}, libraries={}",
                              self->id.to_string(),
                              out.repr_value(self->libraries));
        }
    }
};

}  // namespace dds::crs
