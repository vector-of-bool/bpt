#include "./usage_reqs.hpp"

#include <dds/build/plan/compile_file.hpp>
#include <dds/util/algo.hpp>

#include <spdlog/fmt/fmt.h>

#include <stdexcept>

using namespace dds;

const lm::library* usage_requirement_map::get(std::string ns, std::string name) const noexcept {
    auto found = _reqs.find(library_key{ns, name});
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

std::vector<fs::path> usage_requirement_map::link_paths(std::string ns, std::string name) const {
    auto req = get(ns, name);
    if (!req) {
        throw std::runtime_error(
            fmt::format("Unable to find linking requirement '{}/{}'", ns, name));
    }
    std::vector<fs::path> ret;
    if (req->linkable_path) {
        ret.push_back(*req->linkable_path);
    }
    for (const auto& dep : req->uses) {
        extend(ret, link_paths(dep.namespace_, dep.name));
    }
    for (const auto& link : req->links) {
        extend(ret, link_paths(link.namespace_, link.name));
    }
    return ret;
}
