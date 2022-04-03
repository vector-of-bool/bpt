#pragma once

#include "./dependency.hpp"

#include <bpt/util/name.hpp>

#include <json5/data.hpp>

#include <filesystem>
#include <vector>

namespace dds::crs {

struct library_info {
    dds::name               name;
    std::filesystem::path   path;
    std::vector<dds::name>  intra_using;
    std::vector<dds::name>  intra_test_using;
    std::vector<dependency> dependencies;
    std::vector<dependency> test_dependencies;

    static library_info from_data(const json5::data& data);

    friend void do_repr(auto out, const library_info* self) noexcept {
        out.type("dds::crs::library_info");
        if (self) {
            out.bracket_value(
                "name={}, path={}, intra_using={}, intra_test_using={}, dependencies={}, "
                "test_dependencies={}",
                out.repr_value(self->name),
                out.repr_value(self->path),
                out.repr_value(self->intra_using),
                out.repr_value(self->intra_test_using),
                out.repr_value(self->dependencies),
                out.repr_value(self->test_dependencies));
        }
    }
};

}  // namespace dds::crs