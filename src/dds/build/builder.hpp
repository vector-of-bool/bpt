#pragma once

#include <dds/build/params.hpp>
#include <dds/sdist/dist.hpp>

#include <cassert>
#include <map>

namespace dds {

/**
 * Parameters for building an individual source distribution as part of a larger build plan.
 */
struct sdist_build_params {
    /// The subdirectory in which the source directory should be built
    fs::path subdir;
    /// Whether to build tests
    bool build_tests = false;
    /// Whether to run tests
    bool run_tests = false;
    /// Whether to build applications
    bool build_apps = false;
    /// Whether to enable build warnings
    bool enable_warnings = false;
};

/**
 * Just a pairing of an sdist to the parameters that are used to build it.
 */
struct sdist_target {
    /// The source distribution
    sdist sd;
    /// The build parameters thereof
    sdist_build_params params;
};

/**
 * A builder object. Source distributions are added to the builder, and then they are all built in
 * parallel via `build()`
 */
class builder {
    /// Source distributions that have been added
    std::vector<sdist_target> _sdists;

public:
    /// Add more source distributions
    void add(sdist sd) { add(std::move(sd), sdist_build_params()); }
    void add(sdist sd, sdist_build_params params) {
        _sdists.push_back({std::move(sd), std::move(params)});
    }

    /**
     * Execute the build
     */
    void build(const build_params& params) const;

    /**
     * Compile one or more source files
     */
    void compile_files(const std::vector<fs::path>& files, const build_params& params) const;
};

}  // namespace dds
