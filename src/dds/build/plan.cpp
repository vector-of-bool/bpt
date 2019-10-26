#include "./plan.hpp"

#include <dds/proc.hpp>

#include <range/v3/action/join.hpp>
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

library_plan library_plan::create(const library& lib, const library_build_params& params) {
    std::vector<compile_file_plan>     compile_files;
    std::vector<create_exe_plan>       link_executables;
    std::optional<create_archive_plan> create_archive;
    bool                               should_create_archive = false;

    std::vector<source_file> app_sources;
    std::vector<source_file> test_sources;
    std::vector<source_file> lib_sources;

    auto src_dir = lib.src_dir();
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
            cf_plan.qualifier = lib.manifest().name;
            cf_plan.rules     = params.compile_rules;
            cf_plan.subdir    = fs::path("obj") / lib.manifest().name;
            compile_files.push_back(std::move(cf_plan));
            if (sfile.kind == source_kind::test) {
                test_sources.push_back(sfile);
            } else if (sfile.kind == source_kind::app) {
                app_sources.push_back(sfile);
            } else {
                should_create_archive = true;
                lib_sources.push_back(sfile);
            }
        }
    }

    if (!app_sources.empty() || !test_sources.empty()) {
        spdlog::critical("Apps/tests not implemented on this code path");
    }

    if (should_create_archive) {
        create_archive_plan ar_plan;
        ar_plan.name    = lib.manifest().name;
        ar_plan.out_dir = params.out_subdir;
        create_archive.emplace(std::move(ar_plan));
    }

    return library_plan{lib.manifest().name,
                        lib.path(),
                        params.out_subdir,
                        compile_files,
                        create_archive,
                        link_executables};
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

fs::path create_archive_plan::archive_file_path(const build_env& env) const noexcept {
    return env.output_root / fmt::format("{}{}{}", "lib", name, env.toolchain.archive_suffix());
}

void create_archive_plan::archive(const build_env&             env,
                                  const std::vector<fs::path>& objects) const {
    archive_spec ar;
    ar.input_files   = objects;
    ar.out_path      = archive_file_path(env);
    auto ar_cmd      = env.toolchain.create_archive_command(ar);
    auto out_relpath = fs::relative(ar.out_path, env.output_root).string();

    spdlog::info("[{}] Archive: {}", name, out_relpath);
    auto start_time = std::chrono::steady_clock::now();
    auto ar_res     = run_proc(ar_cmd);
    auto end_time   = std::chrono::steady_clock::now();
    auto dur_ms     = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    spdlog::info("[{}] Archive: {} - {:n}ms", name, out_relpath, dur_ms.count());

    if (!ar_res.okay()) {
        spdlog::error("Creating static library archive failed: {}", out_relpath);
        spdlog::error("Subcommand FAILED: {}\n{}", quote_command(ar_cmd), ar_res.output);
        throw std::runtime_error(
            fmt::format("Creating archive [{}] failed for '{}'", out_relpath, name));
    }
}

namespace {

auto all_libraries(const build_plan& plan) {
    return                                                           //
        plan.build_packages                                          //
        | ranges::views::transform(&package_plan::create_libraries)  //
        | ranges::views::join                                        //
        ;
}

}  // namespace

void build_plan::compile_all(const build_env& env, int njobs) const {
    auto all_compiles =                                           //
        all_libraries(*this)                                      //
        | ranges::views::transform(&library_plan::compile_files)  //
        | ranges::views::join                                     //
        ;

    auto okay = parallel_run(all_compiles, njobs, [&](const auto& cf) { cf.compile(env); });
    if (!okay) {
        throw std::runtime_error("Compilation failed.");
    }
}

void build_plan::archive_all(const build_env& env, int njobs) const {
    parallel_run(all_libraries(*this), njobs, [&](const library_plan& lib) {
        if (!lib.create_archive) {
            return;
        }
        const auto& objects = ranges::views::all(lib.compile_files)  //
            | ranges::views::filter([](auto&& comp) {
                                  return comp.source.kind == source_kind::source;
                              })  //
            | ranges::views::transform(
                                  [&](auto&& comp) { return comp.get_object_file_path(env); })  //
            | ranges::to_vector                                                                 //
            ;
        lib.create_archive->archive(env, objects);
    });
}
