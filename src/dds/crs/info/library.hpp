#pragma once

#include "./dependency.hpp"

#include <dds/util/name.hpp>

#include <json5/data.hpp>

#include <filesystem>
#include <vector>

namespace dds::crs {

struct intra_usage {
    dds::name lib;

    friend void do_repr(auto out, const intra_usage* self) noexcept {
        out.type("dds::crs::intra_usage");
        if (self) {
            out.bracket_value("lib={}", self->lib.str);
        }
    }
};

struct library_info {
    dds::name                name;
    std::filesystem::path    path;
    std::vector<intra_usage> intra_uses;
    std::vector<intra_usage> intra_test_uses;
    std::vector<dependency>  dependencies;
    std::vector<dependency>  test_dependencies;

    static library_info from_data(const json5::data& data);

    friend void do_repr(auto out, const library_info* self) noexcept {
        out.type("dds::crs::library_info");
        if (self) {
            out.bracket_value(
                "name={}, path={}, intra_uses={}, dependencies={}, test_dependencies={}",
                out.repr_value(self->name),
                out.repr_value(self->path),
                out.repr_value(self->intra_uses),
                out.repr_value(self->dependencies),
                out.repr_value(self->test_dependencies));
        }
    }
};

}  // namespace dds::crs