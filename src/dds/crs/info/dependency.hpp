#pragma once

#include <dds/error/nonesuch.hpp>
#include <dds/util/name.hpp>
#include <dds/util/wrap_var.hpp>

#include <json5/data.hpp>
#include <libman/usage.hpp>
#include <pubgrub/interval.hpp>
#include <semver/version.hpp>

#include <string>

namespace dds::crs {

using version_range_set = pubgrub::interval_set<semver::version>;

struct explicit_uses_list {
    std::vector<dds::name> uses;

    bool operator==(explicit_uses_list const&) const noexcept = default;
};

struct implicit_uses_all {
    bool operator==(const implicit_uses_all&) const noexcept = default;
};

struct dependency_uses : sbs::variant_wrapper<implicit_uses_all, explicit_uses_list> {
    using variant_wrapper::variant_wrapper;

    using variant_wrapper::as;
    using variant_wrapper::is;
    using variant_wrapper::visit;
    using variant_wrapper::operator==;
};

struct dependency {
    dds::name         name;
    version_range_set acceptable_versions;
    dependency_uses   uses;

    static dependency from_data(const json5::data&);

    std::string decl_to_string() const noexcept;

    friend void do_repr(auto out, const dependency* self) noexcept {
        out.type("dds::crs::dependency");
        if (self) {
            out.value("{}", self->decl_to_string());
        }
    }
};

}  // namespace dds::crs
