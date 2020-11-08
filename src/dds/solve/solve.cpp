#include "./solve.hpp"

#include <dds/error/errors.hpp>
#include <dds/util/log.hpp>

#include <pubgrub/solve.hpp>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

#include <sstream>

using namespace dds;

namespace {

struct req_type {
    dependency dep;

    using req_ref = const req_type&;

    bool implied_by(req_ref other) const noexcept {
        return dep.versions.contains(other.dep.versions);
    }

    bool excludes(req_ref other) const noexcept {
        return dep.versions.disjoint(other.dep.versions);
    }

    std::optional<req_type> intersection(req_ref other) const noexcept {
        auto range = dep.versions.intersection(other.dep.versions);
        if (range.empty()) {
            return std::nullopt;
        }
        return req_type{dependency{dep.name, std::move(range)}};
    }

    std::optional<req_type> union_(req_ref other) const noexcept {
        auto range = dep.versions.union_(other.dep.versions);
        if (range.empty()) {
            return std::nullopt;
        }
        return req_type{dependency{dep.name, std::move(range)}};
    }

    std::optional<req_type> difference(req_ref other) const noexcept {
        auto range = dep.versions.difference(other.dep.versions);
        if (range.empty()) {
            return std::nullopt;
        }
        return req_type{dependency{dep.name, std::move(range)}};
    }

    auto key() const noexcept { return dep.name; }

    friend bool operator==(req_ref lhs, req_ref rhs) noexcept {
        return lhs.dep.name == rhs.dep.name && lhs.dep.versions == rhs.dep.versions;
    }

    friend std::ostream& operator<<(std::ostream& out, req_ref self) noexcept {
        out << self.dep.to_string();
        return out;
    }
};

auto as_pkg_id(const req_type& req) {
    const version_range_set& versions = req.dep.versions;
    assert(versions.num_intervals() == 1);
    return package_id{req.dep.name, (*versions.iter_intervals().begin()).low};
}

struct solver_provider {
    pkg_id_provider_fn& pkgs_for_name;
    deps_provider_fn&   deps_for_pkg;

    mutable std::map<std::string, std::vector<package_id>> pkgs_by_name = {};

    std::optional<req_type> best_candidate(const req_type& req) const {
        dds_log(debug, "Find best candidate of {}", req.dep.to_string());
        // Look up in the cachce for the packages we have with the given name
        auto found = pkgs_by_name.find(req.dep.name);
        if (found == pkgs_by_name.end()) {
            // If it isn't there, insert an entry in the cache
            found = pkgs_by_name.emplace(req.dep.name, pkgs_for_name(req.dep.name)).first;
        }
        // Find the first package with the version contained by the ranges in the requirement
        auto& for_name = found->second;
        auto  cand = std::find_if(for_name.cbegin(), for_name.cend(), [&](const package_id& pk) {
            return req.dep.versions.contains(pk.version);
        });
        if (cand == for_name.cend()) {
            dds_log(debug, "No candidate for requirement {}", req.dep.to_string());
            return std::nullopt;
        }
        dds_log(debug, "Select candidate {}", cand->to_string());
        return req_type{dependency{cand->name, {cand->version, cand->version.next_after()}}};
    }

    std::vector<req_type> requirements_of(const req_type& req) const {
        dds_log(trace,
                "Lookup requirements of {}@{}",
                req.key(),
                (*req.dep.versions.iter_intervals().begin()).low.to_string());
        auto pk_id = as_pkg_id(req);
        auto deps  = deps_for_pkg(pk_id);
        return deps                                                                          //
            | ranges::views::transform([](const dependency& dep) { return req_type{dep}; })  //
            | ranges::to_vector;
    }
};

using solve_fail_exc = pubgrub::solve_failure_type_t<req_type>;

struct explainer {
    std::stringstream strm;
    bool              at_head = true;

    void put(pubgrub::explain::no_solution) { strm << "Dependencies cannot be satisfied"; }

    void put(pubgrub::explain::dependency<req_type> dep) {
        strm << dep.dependent << " requires " << dep.dependency;
    }

    void put(pubgrub::explain::unavailable<req_type> un) {
        strm << un.requirement << " is not available";
    }

    void put(pubgrub::explain::conflict<req_type> cf) {
        strm << cf.a << " conflicts with " << cf.b;
    }

    void put(pubgrub::explain::needed<req_type> req) { strm << req.requirement << " is required"; }

    void put(pubgrub::explain::disallowed<req_type> dis) {
        strm << dis.requirement << " cannot be used";
    }

    template <typename T>
    void operator()(pubgrub::explain::premise<T> pr) {
        strm.str("");
        put(pr.value);
        dds_log(error, "{} {},", at_head ? "┌─ Given that" : "│         and", strm.str());
        at_head = false;
    }

    template <typename T>
    void operator()(pubgrub::explain::conclusion<T> cncl) {
        at_head = true;
        strm.str("");
        put(cncl.value);
        dds_log(error, "╘═       then {}.", strm.str());
    }

    void operator()(pubgrub::explain::separator) { dds_log(error, ""); }
};

}  // namespace

std::vector<package_id> dds::solve(const std::vector<dependency>& deps,
                                   pkg_id_provider_fn             pkgs_prov,
                                   deps_provider_fn               deps_prov) {
    auto wrap_req
        = deps | ranges::views::transform([](const dependency& dep) { return req_type{dep}; });

    try {
        auto solution = pubgrub::solve(wrap_req, solver_provider{pkgs_prov, deps_prov});
        return solution | ranges::views::transform(as_pkg_id) | ranges::to_vector;
    } catch (const solve_fail_exc& failure) {
        dds_log(error, "Dependency resolution has failed! Explanation:");
        pubgrub::generate_explaination(failure, explainer());
        throw_user_error<errc::dependency_resolve_failure>();
    }
}
