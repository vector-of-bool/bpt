#pragma once

#include <dds/build/plan/library.hpp>

#include <string>
#include <vector>

namespace dds {

/**
 * A package is a top-level component with a name, namespace, and some number of associated
 * libraries. A package plan will roughly correspond to either a source distribution or a project
 * directory
 */
class package_plan {
    /// Package name
    std::string _name;
    /// The libraries in this package
    std::vector<library_plan> _libraries;

public:
    /**
     * Create a new package plan.
     * @param name The name of the package
     */
    package_plan(std::string_view name)
        : _name(name) {}

    /**
     * Add a library plan to this package plan
     * @param lp The `library_plan` to add to the package. Once added, the
     * library plan cannot be changed directly.
     */
    void add_library(library_plan lp) { _libraries.emplace_back(std::move(lp)); }

    /**
     * Get the package name
     */
    auto& name() const noexcept { return _name; }
    /**
     * The libraries in the package
     */
    auto& libraries() const noexcept { return _libraries; }
};

}  // namespace dds