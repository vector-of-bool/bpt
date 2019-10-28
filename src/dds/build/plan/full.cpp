#include "./full.hpp"

#include <dds/build/iter_compilations.hpp>
#include <dds/proc.hpp>

#include <range/v3/action/join.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/repeat_n.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>

#include <spdlog/spdlog.h>

#include <chrono>
#include <mutex>
#include <thread>

using namespace dds;

namespace {

template <typename Range, typename Fn>
bool parallel_run(Range&& rng, int n_jobs, Fn&& fn) {
    // We don't bother with a nice thread pool, as the overhead of most build
    // tasks dwarf the cost of interlocking.
    std::mutex mut;

    auto       iter = rng.begin();
    const auto stop = rng.end();

    std::vector<std::exception_ptr> exceptions;

    auto run_one = [&]() mutable {
        while (true) {
            std::unique_lock lk{mut};
            if (!exceptions.empty()) {
                break;
            }
            if (iter == stop) {
                break;
            }
            auto&& item = *iter++;
            lk.unlock();
            try {
                fn(item);
            } catch (...) {
                lk.lock();
                exceptions.push_back(std::current_exception());
                break;
            }
        }
    };

    std::unique_lock         lk{mut};
    std::vector<std::thread> threads;
    if (n_jobs < 1) {
        n_jobs = std::thread::hardware_concurrency() + 2;
    }
    std::generate_n(std::back_inserter(threads), n_jobs, [&] { return std::thread(run_one); });
    lk.unlock();
    for (auto& t : threads) {
        t.join();
    }
    for (auto eptr : exceptions) {
        try {
            std::rethrow_exception(eptr);
        } catch (const std::exception& e) {
            spdlog::error(e.what());
        }
    }
    return exceptions.empty();
}

}  // namespace

void build_plan::compile_all(const build_env& env, int njobs) const {
    auto okay = parallel_run(iter_compilations(*this), njobs, [&](const compile_file_plan& cf) {
        cf.compile(env);
    });
    if (!okay) {
        throw std::runtime_error("Compilation failed.");
    }
}

void build_plan::archive_all(const build_env& env, int njobs) const {
    auto okay = parallel_run(iter_libraries(*this), njobs, [&](const library_plan& lib) {
        if (lib.create_archive()) {
            lib.create_archive()->archive(env);
        }
    });
    if (!okay) {
        throw std::runtime_error("Error creating static library archives");
    }
}

void build_plan::link_all(const build_env& env, int njobs) const {
    auto executables =         //
        iter_libraries(*this)  //
        | ranges::views::transform([](const library_plan& lib) {
              auto repeated = ranges::views::repeat_n(std::cref(lib), lib.executables().size());
              return ranges::views::zip(repeated, lib.executables());
          })                   //
        | ranges::views::join  //
        ;

    auto okay = parallel_run(executables, njobs, [&](const auto& pair) {
        auto&& [lib, exe] = pair;
        exe.link(env, lib);
    });
    if (!okay) {
        throw std::runtime_error("Failure to link executables");
    }
}

std::vector<test_failure> build_plan::run_all_tests(build_env_ref env, int njobs) const {
    using namespace ranges::views;
    auto test_executables =                       //
        iter_libraries(*this)                     //
        | transform(&library_plan::executables)   //
        | join                                    //
        | filter(&link_executable_plan::is_test)  //
        ;

    std::mutex mut;
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
