#pragma once

#include <dds/pkg/name.hpp>
#include <dds/util/wrap_var.hpp>

#include <libman/usage.hpp>
#include <pubgrub/interval.hpp>
#include <semver/range.hpp>
#include <semver/version.hpp>

#include <string>

namespace dds::crs {

using version_range_set = pubgrub::interval_set<semver::version>;

enum class usage_kind {
    lib,
    test,
    app,
};

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
    usage_kind        kind;
    dependency_uses   uses;

    std::string decl_to_string() const noexcept;

    friend void do_repr(auto out, const dependency* self) noexcept {
        out.type("dds::crs::dependency");
        if (self) {
            out.value("{}", self->decl_to_string());
        }
    }
};

}  // namespace dds::crs
