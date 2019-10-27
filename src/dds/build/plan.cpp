#include "./plan.hpp"

#include <dds/proc.hpp>

#include <range/v3/action/join.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/transform.hpp>

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

namespace {

auto all_libraries(const build_plan& plan) {
    return                                                    //
        plan.packages()                                       //
        | ranges::views::transform(&package_plan::libraries)  //
        | ranges::views::join                                 //
        ;
}

}  // namespace

void build_plan::compile_all(const build_env& env, int njobs) const {
    auto lib_compiles =                                                                        //
        all_libraries(*this)                                                                   //
        | ranges::views::transform(&library_plan::create_archive)                              //
        | ranges::views::filter([&](auto&& opt) { return bool(opt); })                         //
        | ranges::views::transform([&](auto&& opt) -> auto& { return opt->compile_files(); })  //
        | ranges::views::join                                                                  //
        ;

    auto exe_compiles =                                                       //
        all_libraries(*this)                                                  //
        | ranges::views::transform(&library_plan::executables)                //
        | ranges::views::join                                                 //
        | ranges::views::transform(&link_executable_plan::main_compile_file)  //
        ;

    auto all_compiles = ranges::views::concat(lib_compiles, exe_compiles);

    auto okay
        = parallel_run(all_compiles, njobs, [&](const compile_file_plan& cf) { cf.compile(env); });
    if (!okay) {
        throw std::runtime_error("Compilation failed.");
    }
}

void build_plan::archive_all(const build_env& env, int njobs) const {
    parallel_run(all_libraries(*this), njobs, [&](const library_plan& lib) {
        if (lib.create_archive()) {
            lib.create_archive()->archive(env);
        }
    });
}

void build_plan::link_all(const build_env& env, int) const {
    for (auto&& lib : all_libraries(*this)) {
        for (auto&& exe : lib.executables()) {
            exe.link(env, lib);
        }
    }
}
