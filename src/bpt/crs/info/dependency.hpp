#pragma once

#include <bpt/error/nonesuch.hpp>
#include <bpt/util/name.hpp>
#include <bpt/util/wrap_var.hpp>

#include <json5/data.hpp>
#include <libman/usage.hpp>
#include <pubgrub/interval.hpp>
#include <semver/version.hpp>

#include <string>

namespace bpt::crs {

using version_range_set = pubgrub::interval_set<semver::version>;

struct dependency {
    bpt::name              name;
    version_range_set      acceptable_versions;
    std::vector<bpt::name> uses;

    static dependency from_data(const json5::data&);

    std::string decl_to_string() const noexcept;

    friend void do_repr(auto out, const dependency* self) noexcept {
        out.type("bpt::crs::dependency");
        if (self) {
            out.value("{}", self->decl_to_string());
        }
    }
};

}  // namespace bpt::crs
