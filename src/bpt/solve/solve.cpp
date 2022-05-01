#include "./solve.hpp"

#include <bpt/crs/cache_db.hpp>
#include <bpt/crs/info/dependency.hpp>
#include <bpt/dym.hpp>
#include <bpt/error/doc_ref.hpp>
#include <bpt/error/on_error.hpp>
#include <bpt/error/try_catch.hpp>
#include <bpt/util/algo.hpp>
#include <bpt/util/log.hpp>
#include <bpt/util/signal.hpp>

#include <boost/leaf/exception.hpp>
#include <fmt/ostream.h>
#include <neo/overload.hpp>
#include <neo/ranges.hpp>
#include <neo/tl.hpp>
#include <pubgrub/failure.hpp>
#include <pubgrub/solve.hpp>

#include <algorithm>
#include <map>
#include <ranges>
#include <sstream>
#include <string>
#include <tuple>

using namespace bpt;
using namespace bpt::crs;

namespace sr   = std::ranges;
namespace stdv = std::views;

namespace {

/**
 * @brief Given a version range set that only covers a single version, return
 * that single version object
 */
semver::version sole_version(const crs::version_range_set& versions) {
    neo_assert(invariant,
               versions.num_intervals() == 1,
               "sole_version() must only present a single version",
               versions.num_intervals());
    return (*versions.iter_intervals().begin()).low;
}

using name_vec = std::vector<bpt::name>;

struct requirement {
    bpt::name              name;
    crs::version_range_set versions;
    name_vec               uses;
    std::optional<int>     pkg_version;

    explicit requirement(bpt::name              name,
                         crs::version_range_set vrs,
                         name_vec               u,
                         std::optional<int>     pver)
        : name{std::move(name)}
        , versions{std::move(vrs)}
        , uses{std::move(u)}
        , pkg_version{pver} {
        sr::sort(uses);
    }

    static requirement from_crs_dep(const crs::dependency& dep) noexcept {
        return requirement{dep.name, dep.acceptable_versions, dep.uses, std::nullopt};
    }

    auto& key() const noexcept { return name; }

    bool implied_by(const requirement& other) const noexcept {
        if (!versions.contains(other.versions)) {
            return false;
        }
        if (sr::includes(other.uses, uses)) {
            return true;
        }
        return false;
    }

    bool excludes(const requirement& other) const noexcept {
        return versions.disjoint(other.versions);
    }

    name_vec union_usages(name_vec const& l, name_vec const& r) const noexcept {
        name_vec uses_union;
        sr::set_union(l, r, std::back_inserter(uses_union));
        return uses_union;
    }

    name_vec intersect_usages(name_vec const& l, name_vec const& r) const noexcept {
        name_vec uses_intersection;
        sr::set_intersection(l, r, std::back_inserter(uses_intersection));
        return uses_intersection;
    }

    std::optional<requirement> intersection(const requirement& other) const noexcept {
        auto range = versions.intersection(other.versions);
        if (range.empty()) {
            return std::nullopt;
        }
        return requirement{name, std::move(range), union_usages(uses, other.uses), std::nullopt};
    }

    std::optional<requirement> union_(const requirement& other) const noexcept {
        auto range      = versions.union_(other.versions);
        auto uses_isect = intersect_usages(uses, other.uses);
        if (range.empty()) {
            return std::nullopt;
        }
        return requirement{name, std::move(range), std::move(uses_isect), std::nullopt};
    }

    std::optional<requirement> difference(const requirement& other) const noexcept {
        auto range = versions.difference(other.versions);
        if (range.empty() and (uses.empty() or uses == other.uses)) {
            return std::nullopt;
        }
        return requirement{name, std::move(range), union_usages(uses, other.uses), std::nullopt};
    }

    std::string decl_to_string() const noexcept {
        return crs::dependency{name, versions, uses}.decl_to_string();
    }

    friend std::ostream& operator<<(std::ostream& out, const requirement& self) noexcept {
        out << self.decl_to_string();
        return out;
    }

    friend void do_repr(auto out, const requirement* self) noexcept {
        out.type("bpt-requirement");
        if (self) {
            out.value(self->decl_to_string());
        }
    }
};

struct metadata_provider {
    crs::cache_db const& cache_db;

    mutable std::map<std::string, std::vector<crs::cache_db::package_entry>, std::less<>>
        pkgs_by_name{};

    const std::vector<crs::cache_db::package_entry>&
    packages_for_name(std::string_view name) const {
        auto found = pkgs_by_name.find(name);
        if (found == pkgs_by_name.end()) {
            found
                = pkgs_by_name
                      .emplace(std::string(name),
                               cache_db.for_package(bpt::name{std::string(name)}) | neo::to_vector)
                      .first;
            sr::sort(found->second,
                     std::less<>{},
                     NEO_TL(std::make_tuple(_1.pkg.id.version, -_1.pkg.id.revision)));
        }
        return found->second;
    }

    std::optional<requirement> best_candidate(const requirement& req) const {
        bpt::cancellation_point();
        auto& pkgs = packages_for_name(req.name.str);
        bpt_log(debug, "Find best candidate of {}", req.decl_to_string());
        auto cand = sr::find_if(pkgs, [&](auto&& entry) {
            if (!req.versions.contains(entry.pkg.id.version)) {
                return false;
            }
            bool has_all_libraries = sr::all_of(req.uses, [&](auto&& uses_name) {
                return sr::any_of(entry.pkg.libraries, NEO_TL(uses_name == _1.name));
            });
            if (!has_all_libraries) {
                bpt_log(debug,
                        "  Near match: {} (missing one or more required libraries)",
                        entry.pkg.id.to_string());
            }
            return has_all_libraries;
        });

        if (cand == pkgs.cend()) {
            bpt_log(debug, "  (No candidate)");
            return std::nullopt;
        }

        bpt_log(debug, "  Best candidate: {}", cand->pkg.id.to_string());

        return requirement{cand->pkg.id.name,
                           {cand->pkg.id.version, cand->pkg.id.version.next_after()},
                           req.uses,
                           cand->pkg.id.revision};
    }

    /**
     * @brief Look up the requirements of the given package
     *
     * @param req A requirement of a single version of a package, plus the libraries within that
     * package that are required
     * @return std::vector<requirement> The packages that are required
     */
    std::vector<requirement> requirements_of(const requirement& req) const {
        bpt::cancellation_point();
        bpt_log(trace, "Lookup dependencies of {}", req.decl_to_string());
        const auto& version = sole_version(req.versions);
        auto        metas   = cache_db.for_package(req.name, version);
        auto        it      = sr::begin(metas);
        neo_assert(invariant,
                   it != sr::end(metas),
                   "Unexpected empty metadata for requirements of package {}@{}",
                   req.name.str,
                   version.to_string());
        auto pkg = it->pkg;

        std::set<bpt::name> uses;
        extend(uses, req.uses);
        std::set<bpt::name> more_uses;
        while (1) {
            more_uses = uses;
            for (auto& used : uses) {
                auto lib_it = sr::find(pkg.libraries, used, &crs::library_info::name);
                neo_assert(invariant,
                           lib_it != sr::end(pkg.libraries),
                           "Invalid 'using' on non-existent requirement library",
                           used,
                           pkg);
                extend(more_uses, lib_it->intra_using);
            }
            if (sr::includes(uses, more_uses)) {
                break;
            }
            uses = more_uses;
        }

        auto reqs = pkg.libraries                                                //
            | stdv::filter([&](auto&& lib) { return uses.contains(lib.name); })  //
            | stdv::transform(NEO_TL(_1.dependencies))                           //
            | stdv::join                                                         //
            | stdv::transform(NEO_TL(requirement::from_crs_dep(_1)))             //
            | neo::to_vector;
        for (auto&& r : reqs) {
            bpt_log(trace, "  Requires: {}", r.decl_to_string());
        }
        if (reqs.empty()) {
            bpt_log(trace, "  (No dependencies)");
        }
        return reqs;
    }

    void debug(std::string_view sv) const noexcept { bpt_log(trace, "pubgrub: {}", sv); }
};

using solve_failure_exception = pubgrub::solve_failure_type_t<requirement>;

struct fail_explainer {
    std::stringstream part;
    std::stringstream strm;
    bool              at_head = true;

    void put(pubgrub::explain::no_solution) { part << "dependencies cannot be satisfied"; }

    void put(pubgrub::explain::dependency<requirement> dep) {
        fmt::print(part, "{} requires {}", dep.dependent, dep.dependency);
    }

    void put(pubgrub::explain::unavailable<requirement> un) {
        fmt::print(part, "{} is not available", un.requirement);
    }

    void put(pubgrub::explain::conflict<requirement> cf) {
        fmt::print(part, "{} conflicts with {}", cf.a, cf.b);
    }

    void put(pubgrub::explain::needed<requirement> req) {
        fmt::print(part, "{} is required", req.requirement);
    }

    void put(pubgrub::explain::disallowed<requirement> dis) {
        fmt::print(part, "{} cannot be used", dis.requirement);
    }

    void put(pubgrub::explain::compromise<requirement> cmpr) {
        fmt::print(part, "{} and {} agree on {}", cmpr.left, cmpr.right, cmpr.result);
    }

    template <typename T>
    void operator()(pubgrub::explain::premise<T> pr) {
        part.str("");
        put(pr.value);
        fmt::print(strm, "{} {},\n", at_head ? "┌─ Given that" : "│    and that", part.str());
        at_head = false;
    }

    template <typename T>
    void operator()(pubgrub::explain::conclusion<T> cncl) {
        at_head = true;
        part.str("");
        put(cncl.value);
        fmt::print(strm, "╘═       then {}.\n", part.str());
    }

    void operator()(pubgrub::explain::separator) { strm << "\n"; }
};

bpt::e_dependency_solve_failure_explanation
generate_failure_explanation(const solve_failure_exception& exc) {
    fail_explainer explain;
    pubgrub::generate_explaination(exc, explain);
    return e_dependency_solve_failure_explanation{explain.strm.str()};
}

void try_load_nonesuch_packages(boost::leaf::error_id           error,
                                crs::cache_db const&            cache,
                                const std::vector<requirement>& reqs) {
    // Find packages and libraries that aren't at all available
    error.load([&](std::vector<e_nonesuch_package>& missing) {
        for (auto& req : reqs) {
            auto cands = cache.for_package(req.name);
            auto it    = cands.begin();
            if (it == cands.end()) {
                // This requirement has no candidates
                auto all       = cache.all_enabled();
                auto all_names = all
                    | stdv::transform([](auto entry) { return entry.pkg.id.name.str; })
                    | neo::to_vector;
                missing.emplace_back(req.name.str, did_you_mean(req.name.str, all_names));
                continue;
            }
            error.load([&](std::vector<e_nonesuch_using_library>& missing_libs) {
                auto want_libs = req.uses;
                sr::sort(want_libs);
                std::set<std::string> all_lib_names;
                for (; it != cands.end() and not want_libs.empty(); ++it) {
                    auto cand_has_libs = it->pkg.libraries
                        | stdv::transform(&crs::library_info::name) | neo::to_vector;
                    sr::sort(cand_has_libs);
                    std::vector<bpt::name> missing_libs;
                    sr::set_difference(want_libs, cand_has_libs, std::back_inserter(missing_libs));
                    want_libs = std::move(missing_libs);
                    extend(all_lib_names, cand_has_libs | stdv::transform(&bpt::name::str));
                }
                if (not want_libs.empty()) {
                    extend(missing_libs,
                           want_libs | stdv::transform([&](auto&& libname) {
                               return e_nonesuch_using_library{
                                   req.name,
                                   e_nonesuch{libname.str,
                                              did_you_mean(libname.str, all_lib_names)}};
                           }));
                }
            });
        }
    });
}

}  // namespace

std::vector<crs::pkg_id> bpt::solve(crs::cache_db const&                  cache,
                                    neo::any_input_range<crs::dependency> deps_) {
    metadata_provider provider{cache};
    auto deps = deps_ | stdv::transform(NEO_TL(requirement::from_crs_dep(_1))) | neo::to_vector;
    auto sln  = bpt_leaf_try { return pubgrub::solve(deps, provider); }
    bpt_leaf_catch(catch_<solve_failure_exception> exc)->noreturn_t {
        auto error = boost::leaf::new_error();
        try_load_nonesuch_packages(error, cache, deps);
        BOOST_LEAF_THROW_EXCEPTION(error,
                                   bpt::e_dependency_solve_failure{},
                                   BPT_E_ARG(generate_failure_explanation(exc.matched)),
                                   BPT_ERR_REF("dep-res-failure"));
    };
    return sln
        | stdv::transform(NEO_TL(crs::pkg_id{
            .name     = _1.name,
            .version  = sole_version(_1.versions),
            .revision = _1.pkg_version.value(),
        }))
        | neo::to_vector;
}
