#pragma once

#include "./dependency.hpp"

#include <dds/util/name.hpp>

#include <json5/data.hpp>
#include <magic_enum.hpp>

#include <filesystem>
#include <vector>

namespace dds::crs {

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

struct library_info {
    dds::name                name;
    std::filesystem::path    path;
    std::vector<intra_usage> intra_uses;
    std::vector<dependency>  dependencies;

    static library_info from_data(const json5::data& data);

    friend void do_repr(auto out, const library_info* self) noexcept {
        out.type("dds::crs::library_info");
        if (self) {
            out.bracket_value("name={}, path={}, intra_uses={}, dependencies={}",
                              out.repr_value(self->name),
                              out.repr_value(self->path),
                              out.repr_value(self->intra_uses),
                              out.repr_value(self->dependencies));
        }
    }
};

}  // namespace dds::crs