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

lm::library& usage_requirement_map::add(lm::usage ident) {
    auto pair                   = std::pair(ident, lm::library{});
    auto [inserted, did_insert] = _reqs.try_emplace(ident, lm::library());
    if (!did_insert) {
        BOOST_LEAF_THROW_EXCEPTION(e_dup_library_id{ident});
    }
    return inserted->second;
}

usage_requirement_map usage_requirement_map::from_lm_index(const lm::index& idx) noexcept {
    usage_requirement_map ret;
    for (const auto& pkg : idx.packages) {
        for (const auto& lib : pkg.libraries) {
            ret.add(lm::usage{pkg.name, lib.name}, lib);
        }
    }
    return ret;
}

std::vector<fs::path> usage_requirement_map::link_paths(const lm::usage& key) const {
    auto req = get(key);
    if (!req) {
        BOOST_LEAF_THROW_EXCEPTION(e_nonesuch_library{key});
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
    auto                  lib = get(usage);
    if (!lib) {
        BOOST_LEAF_THROW_EXCEPTION(e_nonesuch_library{usage});
    }
    extend(ret, lib->include_paths);
    for (const auto& transitive : lib->uses) {
        extend(ret, include_paths(transitive));
    }
    return ret;
}

namespace {
// The DFS visited status for a vertex
enum class vertex_status {
    unvisited,  // Never seen before
    visiting,   // Currently looking at its children; helps find cycles
    visited,    // Finished examining
};

struct vertex_info {
    vertex_status status = vertex_status::unvisited;
    // When `visiting`, points to the next vertex we are searching.
    // Can be chased to recover the cycle if we find a cycle.
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
                     "Improperly initialized `vertices`; missing vertex for usage.",
                     usage.namespace_,
                     usage.name);

    vertex_info& info = vertices.find(usage)->second;
    if (info.status == vertex_status::visited) {
        // We've already seen this vertex before, so there's no cycle down this path
        return std::nullopt;
    } else if (info.status == vertex_status::visiting) {
        // Found a cycle!
        return usage;
    }

    // Continue searching
    info.status = vertex_status::visiting;
    for (const lm::usage& next : lib.uses) {
        info.next                   = next;
        const lm::library* next_lib = reqs.get(next);

        if (!next_lib) {
            // This is a missing library. This will be an error at a later point, but for now we
            // simply ignore it.
            continue;
        }

        // Recursively search each child vertex.
        if (auto cycle = find_cycle(reqs, vertices, next, *next_lib)) {
            return cycle;
        }
    }
    info.status = vertex_status::visited;

    // We visited everything under this vertex and didn't find a cycle, so there's no cycle down
    // this path.
    return std::nullopt;
}

}  // namespace

std::optional<std::vector<lm::usage>> usage_requirement_map::find_usage_cycle() const {
    // Performs the setup of the DFS and hands off to ::find_cycle() to search particular vertices.

    std::map<lm::usage, vertex_info> vertices;
    // Default construct each.
    for (const auto& [usage, lib] : _reqs) {
        vertices[usage];
    }

    // DFS from each vertex, as we don't have a source vertex.
    // Reuse the same DFS state, so we still visit each vertex only once.
    for (const auto& [usage, lib] : _reqs) {
        if (auto cyclic_usage = find_cycle(*this, vertices, usage, lib)) {
            // Follow `->next`s to recover the cycle.
            std::vector<lm::usage> cycle;
            lm::usage              cur = *cyclic_usage;

            do {
                cycle.push_back(cur);
                cur = vertices.find(cur)->second.next;
            } while (cur != *cyclic_usage);

            return cycle;
        }
    }

    return std::nullopt;
}

void usage_requirements::verify_acyclic() const {
    // Log information on the graph to make it easier to debug issues with the DFS
    dds_log(debug, "Searching for `use` cycles.");
    if (log::level_enabled(log::level::debug)) {
        for (auto const& [lib, deps] : get_usage_map()) {
            const auto uses_str = fmt::format("{}", fmt::join(deps.uses, ", "));
            dds_log(debug, " lib {} uses {}", lib, uses_str);
        }
    }

    std::optional<std::vector<lm::usage>> cycle = get_usage_map().find_usage_cycle();
    if (cycle) {
        neo_assert(invariant,
                   cycle->size() >= 1,
                   "Cycles must have at least one usage.",
                   cycle->size());

        // For error formatting purposes: "a uses b uses a" instead of just "a uses b"
        cycle->push_back(cycle->front());

        write_error_marker("library-json-cyclic-dependency");
        throw_user_error<errc::cyclic_usage>("Cyclic dependency found: {}",
                                             fmt::join(*cycle, " uses "));
    }
}