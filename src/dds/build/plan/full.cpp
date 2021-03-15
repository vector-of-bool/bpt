#include "./full.hpp"

#include <dds/build/iter_compilations.hpp>
#include <dds/build/plan/compile_exec.hpp>
#include <dds/error/errors.hpp>
#include <dds/util/log.hpp>
#include <dds/util/parallel.hpp>
#include <dds/util/range_compat.hpp>

#include <neo/ranges.hpp>
#include <neo/tl.hpp>

#include <range/v3/view/concat.hpp>
#include <range/v3/view/repeat.hpp>
#include <range/v3/view/zip.hpp>

#include <mutex>
#include <thread>

using namespace dds;

namespace {

template <typename T, typename Range>
decltype(auto) pair_up(T& left, Range& right) {
    auto rep = ranges::views::repeat(left);
    return ranges::views::zip(rep, right);
}

}  // namespace

void build_plan::render_all(build_env_ref env) const {
    auto templates = _packages                                        //
        | std::views::transform(NEO_TL(_1.libraries()))               //
        | std::views::join                                            //
        | std::views::transform(NEO_TL(pair_up(_1, _1.templates())))  //
        | std::views::join;
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

void build_plan::compile_files(const build_env&             env,
                               int                          njobs,
                               const std::vector<fs::path>& filepaths) const {
    struct pending_file {
        bool     marked = false;
        fs::path filepath;
    };

    auto as_pending =                                                                   //
        std::views::all(filepaths)                                                      //
        | std::views::transform(NEO_TL(pending_file{false, fs::weakly_canonical(_1)}))  //
        | neo::to_vector;

    auto check_compilation = [&](const compile_file_plan& comp) {
        return std::ranges::any_of(as_pending, [&](pending_file& f) {
            bool same_file = f.filepath == fs::weakly_canonical(comp.source_path());
            if (same_file) {
                f.marked = true;
            }
            return same_file;
        });
    };

    auto comps = iter_compilations(*this) | std::views::filter(check_compilation) | neo::to_vector;

    bool any_unmarked = false;
    auto unmarked     = std::views::filter(as_pending, NEO_TL(not _1.marked));
    for (auto&& um : unmarked) {
        dds_log(error, "Source file [{}] is not compiled by this project", um.filepath.string());
        any_unmarked = true;
    }

    if (any_unmarked) {
        throw_user_error<errc::compile_failure>(
            "One or more requested files is not part of this project (See above)");
    }

    auto okay = dds::compile_all(comps, env, njobs);
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
    // Collect executables that are tests
    auto test_executables =                                //
        iter_libraries(*this)                              //
        | std::views::transform(NEO_TL(_1.executables()))  //
        | std::views::join                                 //
        | std::views::filter(NEO_TL(_1.is_test()))         //
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
