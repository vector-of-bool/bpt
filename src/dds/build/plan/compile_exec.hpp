#pragma once

#include <dds/build/plan/base.hpp>
#include <dds/build/plan/compile_file.hpp>
#include <dds/util/algo.hpp>

#include <functional>
#include <vector>

namespace dds {

namespace detail {

bool compile_all(const ref_vector<const compile_file_plan>& files, build_env_ref env, int njobs);

}  // namespace detail

template <typename Range>
bool compile_all(Range&& rng, build_env_ref env, int njobs) {
    ref_vector<const compile_file_plan> cfps;
    for (auto&& cf : rng) {
        cfps.push_back(cf);
    }
    return detail::compile_all(cfps, env, njobs);
}

}  // namespace dds