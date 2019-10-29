#pragma once

#include <dds/util/fs.hpp>
#include <libman/index.hpp>
#include <libman/library.hpp>

#include <map>
#include <string>

namespace dds {

class shared_compile_file_rules;

class usage_requirement_map {

    struct library_key {
        std::string namespace_;
        std::string name;
    };

    struct library_key_compare {
        bool operator()(const library_key& lhs, const library_key& rhs) const noexcept {
            if (lhs.namespace_ < rhs.namespace_) {
                return true;
            }
            if (lhs.namespace_ > rhs.namespace_) {
                return false;
            }
            if (lhs.name < rhs.name) {
                return true;
            }
            return false;
        }
    };

    std::map<library_key, lm::library, library_key_compare> _reqs;

public:
    const lm::library* get(std::string ns, std::string name) const noexcept;
    lm::library&       add(std::string ns, std::string name);
    void add(std::string ns, std::string name, lm::library lib) { add(ns, name) = lib; }

    void                  apply(shared_compile_file_rules, std::string ns, std::string name) const;
    std::vector<fs::path> link_paths(std::string ns, std::string name) const;

    static usage_requirement_map from_lm_index(const lm::index&) noexcept;
};

}  // namespace dds
