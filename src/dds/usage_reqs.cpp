#include "./usage_reqs.hpp"

#include <dds/build/plan/compile_file.hpp>
#include <dds/util/algo.hpp>

#include <spdlog/fmt/fmt.h>

#include <stdexcept>

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
        throw std::runtime_error(
            fmt::format("More than one library is registered as {}/{}", ns, name));
    }
    return inserted->second;
}

void usage_requirement_map::apply(shared_compile_file_rules rules,
                                  std::string               ns,
                                  std::string               name) const {
    auto reqs = get(ns, name);
    if (!reqs) {
        throw std::runtime_error(
            fmt::format("Unable to resolve usage requirements for '{}/{}'", ns, name));
    }

    for (auto&& use : reqs->uses) {
        apply(rules, use.namespace_, use.name);
    }

    extend(rules.include_dirs(), reqs->include_paths);
    extend(rules.defs(), reqs->preproc_defs);
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
        throw std::runtime_error(
            fmt::format("Unable to find linking requirement '{}/{}'", key.namespace_, key.name));
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
        throw std::runtime_error(
            fmt::format("Cannot find non-existent usage requirements for '{}/{}'",
                        usage.namespace_,
                        usage.name));
    }
    extend(ret, lib->include_paths);
    for (const auto& transitive : lib->uses) {
        extend(ret, include_paths(transitive));
    }
    return ret;
}
