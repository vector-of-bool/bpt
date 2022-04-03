#pragma once

#include <bpt/util/fs/path.hpp>
#include <libman/index.hpp>
#include <libman/library.hpp>

#include <neo/out.hpp>

#include <map>
#include <optional>
#include <string>
#include <utility>

namespace dds {

class shared_compile_file_rules;

struct e_nonesuch_library {
    lm::usage value;
};

struct e_dup_library_id {
    lm::usage value;
};

struct e_cyclic_using {
    std::vector<lm::usage> value;
};

// The underlying map used by the usage requirements
class usage_requirement_map {

    using library_key = lm::usage;

    using _reqs_map_type = std::map<library_key, lm::library>;
    _reqs_map_type _reqs;

public:
    using const_iterator = _reqs_map_type::const_iterator;

    const lm::library* get(const lm::usage& key) const noexcept;
    lm::library&       add(lm::usage u);
    void               add(lm::usage u, lm::library lib) { add(u) = lib; }

    std::vector<fs::path> link_paths(const lm::usage&) const;
    std::vector<fs::path> include_paths(const lm::usage& req) const;

    static usage_requirement_map from_lm_index(const lm::index&) noexcept;

    const_iterator begin() const { return _reqs.begin(); }
    const_iterator end() const { return _reqs.end(); }

    // Returns one of the cycles in the usage dependency graph, if it exists.
    std::optional<std::vector<lm::usage>> find_usage_cycle() const;
};

// The actual usage requirements
class usage_requirements {
    usage_requirement_map _reqs;

    void verify_acyclic() const;

public:
    explicit usage_requirements(usage_requirement_map reqs)
        : _reqs(std::move(reqs)) {
        verify_acyclic();
    }

    const lm::library* get(const lm::usage& key) const noexcept { return _reqs.get(key); }
    const lm::library* get(std::string ns, std::string name) const noexcept {
        return get({ns, name});
    }

    const usage_requirement_map& get_usage_map() const& { return _reqs; }
    usage_requirement_map&&      steal_usage_map() && { return std::move(_reqs); }

    std::vector<fs::path> link_paths(const lm::usage& key) const { return _reqs.link_paths(key); }
    std::vector<fs::path> include_paths(const lm::usage& req) const {
        return _reqs.include_paths(req);
    }

    static usage_requirements from_lm_index(const lm::index& index) noexcept {
        return usage_requirements(usage_requirement_map::from_lm_index(index));
    }
};

}  // namespace dds
