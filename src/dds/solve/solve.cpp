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
        auto found = pkgs_by_name.find(req.dep.name);
        if (found == pkgs_by_name.end()) {
            found = pkgs_by_name.emplace(req.dep.name, pkgs_for_name(req.dep.name)).first;
        }
        auto& vec  = found->second;
        auto  cand = std::find_if(vec.cbegin(), vec.cend(), [&](const package_id& pk) {
            return req.dep.versions.contains(pk.version);
        });
        if (cand == vec.cend()) {
            return std::nullopt;
        }
        return req_type{dependency{cand->name, {cand->version, cand->version.next_after()}}};
    }

    std::vector<req_type> requirements_of(const req_type& req) const {
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
        log::error("{} {},", at_head ? "┌─ Given that" : "│         and", strm.str());
        at_head = false;
    }

    template <typename T>
    void operator()(pubgrub::explain::conclusion<T> cncl) {
        at_head = true;
        strm.str("");
        put(cncl.value);
        log::error("╘═       then {}.", strm.str());
    }

    void operator()(pubgrub::explain::separator) { log::error(""); }
};

}  // namespace

std::vector<package_id> dds::solve(const std::vector<dependency>& deps,
                                   pkg_id_provider_fn             pkgs_prov,
                                   deps_provider_fn               deps_prov) {
    auto wrap_req
        = deps | ranges::v3::views::transform([](const dependency& dep) { return req_type{dep}; });

    try {
        auto solution = pubgrub::solve(wrap_req, solver_provider{pkgs_prov, deps_prov});
        return solution | ranges::views::transform(as_pkg_id) | ranges::to_vector;
    } catch (const solve_fail_exc& failure) {
        log::error("Dependency resolution has failed! Explanation:");
        pubgrub::generate_explaination(failure, explainer());
        throw_user_error<errc::dependency_resolve_failure>();
    }
}
