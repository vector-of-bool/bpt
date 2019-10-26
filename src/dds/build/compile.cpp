#include "./compile.hpp"

#include <dds/proc.hpp>
#include <dds/util/algo.hpp>
#include <dds/util/signal.hpp>

#include <spdlog/spdlog.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

using namespace dds;

void compile_file_plan::compile(const build_env& env) const {
    const auto obj_path = get_object_file_path(env);
    fs::create_directories(obj_path.parent_path());

    spdlog::info("[{}] Compile: {}",
                 qualifier,
                 fs::relative(source.path, source.basis_path).string());
    auto start_time = std::chrono::steady_clock::now();

    compile_file_spec spec{source.path, obj_path};
    spec.enable_warnings = rules.enable_warnings();

    extend(spec.include_dirs, rules.include_dirs());
    extend(spec.definitions, rules.defs());

    auto cmd         = env.toolchain.create_compile_command(spec);
    auto compile_res = run_proc(cmd);

    auto end_time = std::chrono::steady_clock::now();
    auto dur_ms   = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    spdlog::info("[{}] Compile: {} - {:n}ms",
                 qualifier,
                 fs::relative(source.path, source.basis_path).string(),
                 dur_ms.count());

    if (!compile_res.okay()) {
        spdlog::error("Compilation failed: {}", source.path.string());
        spdlog::error("Subcommand FAILED: {}\n{}", quote_command(cmd), compile_res.output);
        throw compile_failure(fmt::format("Compilation failed for {}", source.path.string()));
    }

    // MSVC prints the filename of the source file. Dunno why, but they do.
    if (compile_res.output.find(spec.source_path.filename().string() + "\r\n") == 0) {
        compile_res.output.erase(0, spec.source_path.filename().string().length() + 2);
    }

    if (!compile_res.output.empty()) {
        spdlog::warn("While compiling file {} [{}]:\n{}",
                     spec.source_path.string(),
                     quote_command(cmd),
                     compile_res.output);
    }
}

fs::path compile_file_plan::get_object_file_path(const build_env& env) const noexcept {
    auto relpath = fs::relative(source.path, source.basis_path);
    auto ret     = env.output_root / subdir / relpath;
    ret.replace_filename(relpath.filename().string() + env.toolchain.object_suffix());
    return ret;
}

void dds::execute_all(const std::vector<compile_file_plan>& compilations,
                      int                                   n_jobs,
                      const build_env&                      env) {
    // We don't bother with a nice thread pool, as the overhead of compiling
    // source files dwarfs the cost of interlocking.
    std::mutex mut;

    auto       comp_iter = compilations.begin();
    const auto end_iter  = compilations.end();

    std::vector<std::exception_ptr> exceptions;

    auto compile_one = [&]() mutable {
        while (true) {
            std::unique_lock lk{mut};
            if (!exceptions.empty()) {
                break;
            }
            if (comp_iter == end_iter) {
                break;
            }
            auto& compilation = *comp_iter++;
            lk.unlock();
            try {
                compilation.compile(env);
                cancellation_point();
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
    std::generate_n(std::back_inserter(threads), n_jobs, [&] { return std::thread(compile_one); });
    spdlog::info("Parallel compile with {} threads", threads.size());
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
    if (!exceptions.empty()) {
        throw compile_failure("Failed to compile library sources");
    }
}
