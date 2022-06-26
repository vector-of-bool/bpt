#include "../options.hpp"

#include "./build_common.hpp"
#include <bpt/error/errors.hpp>
#include <bpt/error/marker.hpp>
#include <bpt/error/nonesuch.hpp>
#include <bpt/util/log.hpp>

#include <boost/leaf/pred.hpp>
#include <fansi/styled.hpp>

using namespace fansi::literals;

namespace bpt::cli::cmd {

int _compile_file(const options& opts) {
    return boost::leaf::try_catch(
        [&] {
            auto builder = create_project_builder(opts);
            builder.compile_files(opts.compile_file.files,
                                  {
                                      .out_root
                                      = opts.out_path.value_or(fs::current_path() / "_build"),
                                      .emit_built_json = std::nullopt,
                                      .tweaks_dir      = opts.build.tweaks_dir,
                                      .toolchain       = opts.load_toolchain(),
                                      .parallel_jobs   = opts.jobs,
                                  });
            return 0;
        },
        [&](boost::leaf::catch_<bpt::user_error<bpt::errc::compile_failure>>,
            std::vector<e_nonesuch> missing) {
            if (missing.size() == 1) {
                bpt_log(
                    error,
                    "Requested source file [.bold.red[{}]] is not a file compiled for this project"_styled,
                    missing.front().given);
            } else {
                bpt_log(error, "The following files are not compiled as part of this project:");
                for (auto&& f : missing) {
                    bpt_log(error, "  - .bold.red[{}]"_styled, f.given);
                }
            }
            write_error_marker("nonesuch-compile-file");
            return 2;
        },
        [&](boost::leaf::catch_<bpt::user_error<bpt::errc::compile_failure>> e) {
            bpt_log(error, e.matched.what());
            bpt_log(error, "  (Refer to compiler output for more information)");
            write_error_marker("compile-file-failed");
            return 2;
        });
}

int compile_file(const options& opts) {
    return handle_build_error([&] { return _compile_file(opts); });
}

}  // namespace bpt::cli::cmd
