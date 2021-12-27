#include "./solve2.hpp"

#include <dds/crs/cache_db.hpp>
#include <dds/crs/dependency.hpp>
#include <dds/error/on_error.hpp>
#include <dds/error/try_catch.hpp>
#include <dds/util/algo.hpp>
#include <dds/util/log.hpp>
#include <dds/util/signal.hpp>

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

using namespace dds;
using namespace dds::crs;

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

struct requirement {
    crs::dependency dep;

    explicit requirement(crs::dependency d)
        : dep(std::move(d)) {
        d.uses.visit(neo::overload{
            [](explicit_uses_list& l) { std::ranges::sort(l.uses); },
            [](implicit_uses_all) {},
        });
    }

    std::string key() const noexcept { return dep.name.str; }

    auto tie() const noexcept { return std::tie(dep.name, dep.acceptable_versions, dep.uses); }

    friend bool operator==(const requirement& lhs, const requirement& rhs) noexcept {
        return lhs.tie() == rhs.tie();
    }

    auto& versions() const noexcept { return dep.acceptable_versions; }

    bool implied_by(const requirement& other) const noexcept {
        return versions().contains(other.versions());
    }

    bool excludes(const requirement& other) const noexcept {
        return versions().disjoint(other.versions());
    }

    dependency_uses union_usages(dependency_uses const& l,
                                 dependency_uses const& r) const noexcept {
        return l.visit(neo::overload{
            [&](explicit_uses_list const& left) -> dependency_uses {
                return r.visit(neo::overload{
                    [&](explicit_uses_list const& right) -> dependency_uses {
                        explicit_uses_list uses_union;
                        std::ranges::set_union(left.uses,
                                               right.uses,
                                               std::back_inserter(uses_union.uses));
                        return uses_union;
                    },
                    [](implicit_uses_all a) -> dependency_uses { return a; },
                });
            },
            [&](implicit_uses_all a) -> dependency_uses { return a; },
        });
    }

    std::optional<requirement> intersection(const requirement& other) const noexcept {
        auto range = versions().intersection(other.versions());
        if (range.empty()) {
            return std::nullopt;
        }
        return requirement{crs::dependency{dep.name,
                                           std::move(range),
                                           dep.kind,
                                           union_usages(dep.uses, other.dep.uses)}};
    }

    std::optional<requirement> union_(const requirement& other) const noexcept {
        auto range = versions().union_(other.versions());
        if (range.empty()) {
            return std::nullopt;
        }
        return requirement{crs::dependency{dep.name,
                                           std::move(range),
                                           dep.kind,
                                           union_usages(dep.uses, other.dep.uses)}};
    }

    std::optional<requirement> difference(const requirement& other) const noexcept {
        auto range = versions().difference(other.versions());
        if (range.empty()) {
            return std::nullopt;
        }
        return requirement{crs::dependency{dep.name, std::move(range), dep.kind, dep.uses}};
    }

    friend std::ostream& operator<<(std::ostream& out, const requirement& self) noexcept {
        out << self.dep.decl_to_string();
        return out;
    }
};

struct metadata_provider {
    crs::cache_db const& cache_db;

    mutable std::map<std::string, std::vector<crs::cache_db::package_entry>> pkgs_by_name{};

    std::optional<requirement> best_candidate(const requirement& req) const {
        dds::cancellation_point();
        auto found = pkgs_by_name.find(req.dep.name.str);
        dds_log(trace, "Find best candidate of {}", req.dep.decl_to_string());
        if (found == pkgs_by_name.end()) {
            found = pkgs_by_name  //
                        .emplace(req.dep.name.str, cache_db.for_package(req.dep.name))
                        .first;
            std::ranges::sort(found->second,
                              std::less<>{},
                              NEO_TL(std::tuple(_1.pkg.version, -_1.pkg.meta_version)));
        }
        auto cand
            = std::ranges::find_if(found->second,
                                   NEO_TL(req.dep.acceptable_versions.contains(_1.pkg.version)));

        if (cand == found->second.cend()) {
            dds_log(trace, "  (No candidate)");
            return std::nullopt;
        }
        dds_log(trace,
                "  Best candidate: {}@{}",
                cand->pkg.name.str,
                cand->pkg.version.to_string());
        return requirement{crs::dependency{
            cand->pkg.name,
            {cand->pkg.version, cand->pkg.version.next_after()},
            req.dep.kind,
            req.dep.uses,
        }};
    }

    /**
     * @brief Look up the requirements of the given package
     *
     * @param req A requirement of a single version of a package, plus the libraries within that
     * package that are required
     * @return std::vector<requirement> The packages that are required
     */
    std::vector<requirement> requirements_of(const requirement& req) const {
        dds::cancellation_point();
        dds_log(trace,
                "Lookup dependencies of {}@{}",
                req.dep.name.str,
                sole_version(req.dep.acceptable_versions).to_string());
        const auto& version = sole_version(req.dep.acceptable_versions);
        auto        metas   = cache_db.for_package(req.dep.name, version);
        neo_assert(invariant,
                   !metas.empty(),
                   "Unexpected empty metadata for requirements of package {}@{}",
                   req.dep.name.str,
                   version.to_string());
        // The package that is required:
        auto& pkg = metas.front().pkg;
        DDS_E_SCOPE(pkg);
        DDS_E_SCOPE(req.dep);

        std::set<dds::name> use_from_pkg = req.dep.uses.visit(neo::overload{
            [&](implicit_uses_all) {
                std::set<dds::name> r;
                extend(r, pkg.libraries | std::views::transform(NEO_TL(_1.name)));
                return r;
            },
            [&](explicit_uses_list const& seq) {
                return std::set<dds::name>{seq.uses.cbegin(), seq.uses.cend()};
            },
        });

        while (true) {
            std::set<dds::name> use_more;
            for (auto&& use : use_from_pkg) {
                dds_log(trace, "  Uses library: {}", use);
                DDS_E_SCOPE(use);
                /** Find the library meta for the used library */
                auto dep_lib = std::ranges::find_if(pkg.libraries, NEO_TL(_1.name == use));
                if (dep_lib == pkg.libraries.cend()) {
                    BOOST_LEAF_THROW_EXCEPTION(e_usage_no_such_lib{});
                }
                dds::extend(use_more,
                            dep_lib->intra_uses
                                | std::views::filter(NEO_TL(_1.kind == dds::crs::usage_kind::lib))
                                | std::views::transform(NEO_TL(_1.lib)));
            }
            if (std::ranges::includes(use_from_pkg, use_more)) {
                break;
            }
            dds_log(trace,
                    "  Use: {}",
                    dds::joinstr(", ", use_from_pkg | std::views::transform(NEO_TL(_1.str))));
            extend(use_from_pkg, use_more);
            use_more.clear();
        }

        std::vector<requirement> ret;
        for (dds::name intra_used : use_from_pkg) {
            auto dep_lib = std::ranges::find_if(pkg.libraries, NEO_TL(_1.name == intra_used));
            neo_assert(invariant,
                       dep_lib != pkg.libraries.end(),
                       "Invalid intra-package usage was uncaught",
                       intra_used.str,
                       pkg.name.str);
            dds::extend(ret,
                        dep_lib->dependencies
                            | std::views::filter(NEO_TL(_1.kind == dds::crs::usage_kind::lib))
                            | std::views::transform(NEO_TL(requirement{_1})));
        }
        return ret;
    }
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

dds::e_dependency_solve_failure_explanation
generate_failure_explanation(const solve_failure_exception& exc) {
    // TODO: Implement a new explainer
    fail_explainer explain;
    pubgrub::generate_explaination(exc, explain);
    return e_dependency_solve_failure_explanation{explain.strm.str()};
}

}  // namespace

std::vector<crs::pkg_id> dds::solve2(crs::cache_db const&                  cache,
                                     neo::any_input_range<crs::dependency> deps_) {
    auto deps = deps_ | std::views::transform(NEO_TL(requirement{_1})) | neo::to_vector;
    metadata_provider provider{cache};
    auto              sln = dds_leaf_try { return pubgrub::solve(deps, provider); }
    dds_leaf_catch(catch_<solve_failure_exception> exc)->noreturn_t {
        BOOST_LEAF_THROW_EXCEPTION(dds::e_dependency_solve_failure{},
                                   DDS_E_ARG(generate_failure_explanation(exc.matched)));
    };
    return sln
        | std::views::transform(NEO_TL(crs::pkg_id{
            .name         = _1.dep.name,
            .version      = sole_version(_1.dep.acceptable_versions),
            .meta_version = 0,
        }))
        | neo::to_vector;
}
