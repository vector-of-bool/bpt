#pragma once

#include <bpt/build/plan/exe.hpp>
#include <bpt/build/plan/package.hpp>

namespace dds {

/**
 * Encompases an entire build plan.
 *
 * A build plan consists of some number of packages, defined as `package_plan`
 * objects.
 */
class build_plan {
    /// The packages that are part of this plan.
    std::vector<package_plan> _packages;

public:
    /**
     * Append a new package plan. Returns a reference to the package plan so that it can be further
     * tweaked. Note that the reference is not stable.
     */
    package_plan& add_package(package_plan p) noexcept {
        return _packages.emplace_back(std::move(p));
    }

    /**
     * All of the packages in this plan
     */
    auto& packages() const noexcept { return _packages; }
    /**
     * Compile all files in the plan.
     */
    void compile_all(const build_env& env, int njobs) const;
    /**
     * Generate all static library archive in the plan
     */
    void archive_all(const build_env& env, int njobs) const;
    /**
     * Link all runtime binaries (executables) in the plan
     */
    void link_all(const build_env& env, int njobs) const;

    /**
     * Compile the files given in the vector of file paths.
     */
    void compile_files(const build_env& env, int njobs, const std::vector<fs::path>& paths) const;

    /**
     * Execute all tests defined in the plan. Returns information for every failed test.
     */
    std::vector<test_failure> run_all_tests(build_env_ref env, int njobs) const;
};

}  // namespace dds
