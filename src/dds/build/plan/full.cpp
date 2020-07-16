#include "./full.hpp"

#include <dds/build/iter_compilations.hpp>
#include <dds/build/plan/compile_exec.hpp>
#include <dds/error/errors.hpp>
#include <dds/util/parallel.hpp>

#include <range/v3/view/concat.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/repeat.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>

#include <mutex>
#include <thread>

using namespace dds;

namespace {

template <typename T, typename Range>
decltype(auto) pair_up(T& left, Range& right) {
    auto rep = ranges::view::repeat(left);
    return ranges::view::zip(rep, right);
}

}  // namespace

void build_plan::render_all(build_env_ref env) const {
    auto templates = _packages                                                                    //
        | ranges::view::transform(&package_plan::libraries)                                       //
        | ranges::view::join                                                                      //
        | ranges::view::transform([](const auto& lib) { return pair_up(lib, lib.templates()); })  //
        | ranges::view::join;
    for (const auto& [lib, tmpl] : templates) {
        tmpl.render(env, lib.library_());
    }
}

void build_plan::compile_all(const build_env& env, int njobs) const {
    auto okay = dds::compile_all(iter_compilations(*this), env, njobs);
    if (!okay) {
        throw_user_error<errc::compile_failure>();
    }
}

void build_plan::archive_all(const build_env& env, int njobs) const {
    auto okay = parallel_run(iter_libraries(*this), njobs, [&](const library_plan& lib) {
        if (lib.archive_plan()) {
            lib.archive_plan()->archive(env);
        }
    });
    if (!okay) {
        throw_external_error<errc::archive_failure>();
    }
}

void build_plan::link_all(const build_env& env, int njobs) const {
    // Generate a pairing between executables and the libraries that own them
    std::vector<std::pair<std::reference_wrapper<const library_plan>,
                          std::reference_wrapper<const link_executable_plan>>>
        executables;
    for (auto&& lib : iter_libraries(*this)) {
        for (auto&& exe : lib.executables()) {
            executables.emplace_back(lib, exe);
        }
    }

    auto okay = parallel_run(executables, njobs, [&](const auto& pair) {
        auto&& [lib, exe] = pair;
        exe.get().link(env, lib);
    });
    if (!okay) {
        throw_user_error<errc::link_failure>();
    }
}

std::vector<test_failure> build_plan::run_all_tests(build_env_ref env, int njobs) const {
    using namespace ranges::views;
    // Collect executables that are tests
    auto test_executables =                       //
        iter_libraries(*this)                     //
        | transform(&library_plan::executables)   //
        | join                                    //
        | filter(&link_executable_plan::is_test)  //
        ;

    std::mutex                mut;
    std::vector<test_failure> fails;

    parallel_run(test_executables, njobs, [&](const auto& exe) {
        auto fail_info = exe.run_test(env);
        if (fail_info) {
            std::scoped_lock lk{mut};
            fails.emplace_back(std::move(*fail_info));
        }
    });
    return fails;
}
