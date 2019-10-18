#include "./compile.hpp"

#include <dds/proc.hpp>

#include <spdlog/spdlog.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

using namespace dds;

void file_compilation::compile(const toolchain& tc) const {
    fs::create_directories(obj.parent_path());

    spdlog::info("[{}] Compile file: {}",
                 owner_name,
                 fs::relative(source.path, rules.base_path()).string());
    auto start_time = std::chrono::steady_clock::now();

    compile_file_spec spec{source.path, obj};
    spec.enable_warnings = enable_warnings;

    extend(spec.include_dirs, rules.include_dirs());
    extend(spec.definitions, rules.defs());

    auto cmd         = tc.create_compile_command(spec);
    auto compile_res = run_proc(cmd);

    auto end_time = std::chrono::steady_clock::now();
    auto dur_ms   = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    spdlog::info("[{}] Compile file: {} - {:n}ms",
                 owner_name,
                 fs::relative(source.path, rules.base_path()).string(),
                 dur_ms.count());

    if (!compile_res.okay()) {
        spdlog::error("Compilation failed: {}", source.path.string());
        spdlog::error("Subcommand FAILED: {}\n{}", quote_command(cmd), compile_res.output);
        throw compile_failure("Compilation failed.");
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

void compilation_set::execute_all(const toolchain& tc, int n_jobs) const {
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
                compilation.compile(tc);
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