#include "./usage_reqs.hpp"

#include <dds/build/plan/compile_file.hpp>
#include <dds/error/errors.hpp>
#include <dds/util/algo.hpp>
#include <dds/util/log.hpp>
#include <dds/util/result.hpp>

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <range/v3/view/transform.hpp>

#include <map>
#include <stdexcept>
#include <string_view>

using namespace dds;

const lm::library* usage_requirement_map::get(const lm::usage& key) const noexcept {
    auto found = _reqs.find(key);
    if (found == _reqs.end()) {
        return nullptr;
    }
    return &found->second;
}

lm::library& usage_requirement_map::add(std::string ns, std::string name) {
    auto pair                   = std::pair(library_key{ns, name}, lm::library{});
    auto [inserted, did_insert] = _reqs.try_emplace(library_key{ns, name}, lm::library());
    if (!did_insert) {
        throw_user_error<errc::dup_lib_name>("More than one library is registered as `{}/{}'",
                                             ns,
                                             name);
    }
    return inserted->second;
}

usage_requirement_map usage_requirement_map::from_lm_index(const lm::index& idx) noexcept {
    usage_requirement_map ret;
    for (const auto& pkg : idx.packages) {
        for (const auto& lib : pkg.libraries) {
            ret.add(pkg.namespace_, lib.name, lib);
        }
    }
    return ret;
}

std::vector<fs::path> usage_requirement_map::link_paths(const lm::usage& key) const {
    auto req = get(key);
    if (!req) {
        throw_user_error<errc::unknown_usage_name>("Unable to find linking requirement '{}/{}'",
                                                   key.namespace_,
                                                   key.name);
    }
    std::vector<fs::path> ret;
    if (req->linkable_path) {
        ret.push_back(*req->linkable_path);
    }
    for (const auto& dep : req->uses) {
        extend(ret, link_paths(dep));
    }
    for (const auto& link : req->links) {
        extend(ret, link_paths(link));
    }
    return ret;
}

std::vector<fs::path> usage_requirement_map::include_paths(const lm::usage& usage) const {
    std::vector<fs::path> ret;
    auto                  lib = get(usage.namespace_, usage.name);
    if (!lib) {
        throw_user_error<
            errc::unknown_usage_name>("Cannot find non-existent usage requirements for '{}/{}'",
                                      usage.namespace_,
                                      usage.name);
    }
    extend(ret, lib->include_paths);
    for (const auto& transitive : lib->uses) {
        extend(ret, include_paths(transitive));
    }
    return ret;
}

namespace {
enum class vertex_status {
    unvisited,  // Never seen before
    visiting,   // Currently looking at its children
    visited,    // Finished examining
};

struct vertex_info {
    vertex_status status = vertex_status::unvisited;
    // Only relevant when `visiting`, used for recovering the cycle when we find it.
    lm::usage next;
};

// Implements a recursive DFS.
// Returns a lm::usage in a cycle if it exists.
// The `vertex_info->next`s can be examined to recover the cycle.
template <typename UsageCmp>
std::optional<lm::usage> find_cycle(const usage_requirement_map&                reqs,
                                    std::map<lm::usage, vertex_info, UsageCmp>& vertices,
                                    const lm::usage&                            usage,
                                    const lm::library&                          lib) {
    neo_assert_audit(invariant,
                     vertices.find(usage) != vertices.end(),
                     "Improperly initialized `vertices`.");

    vertex_info& info = vertices.find(usage)->second;
    if (info.status == vertex_status::visiting) {
        // Found a cycle!
        return usage;
    } else if (info.status == vertex_status::visited) {
        return std::nullopt;
    }

    info.status = vertex_status::visiting;
    for (const lm::usage& next : lib.uses) {
        info.next                   = next;
        const lm::library* next_lib = reqs.get(next);

        neo_assert(invariant, next_lib, "Unaccounted for `uses`.");

        if (auto cycle = find_cycle(reqs, vertices, next, *next_lib)) {
            return cycle;
        }
    }
    info.status = vertex_status::visited;

    return std::nullopt;
}

}  // namespace

std::optional<std::vector<lm::usage>> usage_requirement_map::find_usage_cycle() const {
    std::map<lm::usage, vertex_info, library_key_compare> vertices;
    // Default construct each.
    for (const auto& [usage, lib] : _reqs) {
        vertices[usage];
    }

    // DFS from each vertex, as we don't have a source vertex.
    for (const auto& [usage, lib] : _reqs) {
        if (auto cyclic_usage = find_cycle(*this, vertices, usage, lib)) {
            // Follow `->next`s to recover the cycle.
            std::vector<lm::usage> cycle;
            lm::usage              cur = *cyclic_usage;

            const auto eq = [](const lm::usage& lhs, const lm::usage& rhs) -> bool {
                return lhs.namespace_ == rhs.namespace_ && lhs.name == rhs.name;
            };

            do {
                cycle.push_back(cur);
                cur = vertices.find(cur)->second.next;
            } while (!eq(cur, *cyclic_usage));

            return cycle;
        }
    }

    return std::nullopt;
}

void usage_requirements::verify_acyclic() const {
    dds_log(debug, "Searching for `use` cycles.");
    if (log::level_enabled(log::level::debug)) {
        for (auto const& [k, v] : get_usage_map()) {
            std::vector<std::string> uses;
            extend(uses, v.uses | ranges::views::transform([](const lm::usage& usage) {
                             return fmt::format("'{}/{}'", usage.namespace_, usage.name);
                         }));
            const auto uses_str = fmt::format("{}", fmt::join(uses, ", "));
            dds_log(debug, " lib '{}/{}' uses {}", k.namespace_, k.name, uses_str);
        }
    }

    std::optional<std::vector<lm::usage>> cycle = get_usage_map().find_usage_cycle();
    if (cycle) {
        neo_assert(invariant, cycle->size() >= 1, "Cycles must have at least one usage.");

        // For error formatting purposes, a uses b uses a instead of just a uses b
        cycle->push_back(cycle->front());

        write_error_marker("library-json-cyclic-dependency");
        throw_user_error<errc::cyclic_usage>(
            "Cyclic dependency found: {}",
            fmt::join(*cycle | ranges::views::transform([](const lm::usage& usage) {
                return fmt::format("'{}/{}'", usage.namespace_, usage.name);
            }),
                      " uses "));
    }
}