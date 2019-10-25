#include "./plan.hpp"

#include <range/v3/action/join.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/repeat_n.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>

#include <spdlog/spdlog.h>

#include <mutex>
#include <thread>

using namespace dds;

void build_plan::add_sroot(const sroot& root, const sroot_build_params& params) {
    create_libraries.push_back(library_plan::create(root, params));
}

library_plan library_plan::create(const sroot& root, const sroot_build_params& params) {
    std::vector<compile_file_plan>   compile_files;
    std::vector<create_archive_plan> create_archives;
    std::vector<create_exe_plan>     link_executables;

    std::vector<source_file> app_sources;
    std::vector<source_file> test_sources;
    std::vector<source_file> lib_sources;

    auto src_dir = root.src_dir();
    if (src_dir.exists()) {
        auto all_sources = src_dir.sources();
        auto to_compile  = all_sources | ranges::views::filter([&](const source_file& sf) {
                              return (sf.kind == source_kind::source
                                      || (sf.kind == source_kind::app && params.build_apps)
                                      || (sf.kind == source_kind::test && params.build_tests));
                          });

        for (const auto& sfile : to_compile) {
            compile_file_plan cf_plan;
            cf_plan.source    = sfile;
            cf_plan.qualifier = params.main_name;
            cf_plan.rules     = params.compile_rules;
            compile_files.push_back(std::move(cf_plan));
            if (sfile.kind == source_kind::test) {
                test_sources.push_back(sfile);
            } else if (sfile.kind == source_kind::app) {
                app_sources.push_back(sfile);
            } else {
                lib_sources.push_back(sfile);
            }
        }
    }

    if (!app_sources.empty() || !test_sources.empty()) {
        assert(false && "Apps/tests not implemented on this code path");
    }

    if (!lib_sources.empty()) {
        create_archive_plan ar_plan;
        ar_plan.in_sources = lib_sources                                   //
            | ranges::views::transform([](auto&& sf) { return sf.path; })  //
            | ranges::to_vector;
        ar_plan.name    = params.main_name;
        ar_plan.out_dir = params.out_dir;
        create_archives.push_back(std::move(ar_plan));
    }

    return library_plan{compile_files, create_archives, link_executables, params.out_dir};
}

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

void build_plan::compile_all(const toolchain& tc, int njobs, path_ref out_prefix) const {
    std::vector<std::pair<fs::path, std::reference_wrapper<const compile_file_plan>>> comps;
    for (const auto& lib : create_libraries) {
        const auto lib_out_prefix = out_prefix / lib.out_subdir;
        for (auto&& cf_plan : lib.compile_files) {
            comps.emplace_back(lib_out_prefix, cf_plan);
        }
    }

    auto okay = parallel_run(comps, njobs, [&](const auto& pair) {
        const auto& [out_dir, cf_plan] = pair;
        cf_plan.get().compile(tc, out_dir);
    });
    if (!okay) {
        throw std::runtime_error("Compilation failed.");
    }
}