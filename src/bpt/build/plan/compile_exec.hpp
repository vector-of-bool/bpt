#pragma once

#include <bpt/build/plan/base.hpp>
#include <bpt/build/plan/compile_file.hpp>
#include <bpt/util/algo.hpp>

#include <functional>
#include <vector>

namespace dds {

namespace detail {

bool compile_all(const ref_vector<const compile_file_plan>& files, build_env_ref env, int njobs);

}  // namespace detail

/**
 * Compiles all files in the given range of `compile_file_plan`. Uses as much
 * parallelism as specified by `njobs`.
 * @param rng The file compilation plans to execute
 * @param env The build environment in which the compilations will execute
 * @param njobs The maximum number of parallel compilations to execute at once.
 * @returns `true` if all compilations were successful, `false` otherwise.
 */
template <typename Range>
bool compile_all(Range&& rng, build_env_ref env, int njobs) {
    ref_vector<const compile_file_plan> cfps;
    for (auto&& cf : rng) {
        cfps.push_back(cf);
    }
    return detail::compile_all(cfps, env, njobs);
}

}  // namespace dds