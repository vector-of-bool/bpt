#pragma once

#include "./dependency.hpp"

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

struct intra_usage {
    dds::name  lib;
    usage_kind kind;

    friend void do_repr(auto out, const intra_usage* self) noexcept {
        out.type("dds::crs::intra_usage");
        if (self) {
            out.bracket_value("lib={}, kind={}", self->lib.str, magic_enum::enum_name(self->kind));
        }
    }
};

struct library_meta {
    dds::name                name;
    std::filesystem::path    path;
    std::vector<intra_usage> intra_uses;
    std::vector<dependency>  dependencies;

    friend void do_repr(auto out, const library_meta* self) noexcept {
        out.type("dds::crs::library_meta");
        if (self) {
            out.bracket_value("name={}, path={}, intra_uses={}, dependencies={}",
                              out.repr_value(self->name),
                              out.repr_value(self->path),
                              out.repr_value(self->intra_uses),
                              out.repr_value(self->dependencies));
        }
    }
};

struct package_meta {
    dds::name                 name;
    dds::name                 namespace_;
    semver::version           version;
    int                       meta_version = 0;
    std::vector<library_meta> libraries;
    json5::data               extra;

    static package_meta from_json_data_v1(const json5::data&);
    static package_meta from_json_data(const json5::data&);
    static package_meta from_json_str(std::string_view json);

    void throw_if_invalid() const;

    std::string to_json(int indent) const noexcept;
    std::string to_json() const noexcept { return to_json(0); }

    friend void do_repr(auto out, const package_meta* self) noexcept {
        out.type("dds::crs::package_meta");
        if (self) {
            out.bracket_value("name={}, namespace={}, version={}, meta_version={}, libraries={}",
                              out.repr_value(self->name),
                              out.repr_value(self->namespace_),
                              out.repr_value(self->version.to_string()),
                              out.repr_value(self->meta_version),
                              out.repr_value(self->libraries));
        }
    }
};

}  // namespace dds::crs
