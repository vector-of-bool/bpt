#include "../options.hpp"

#include "./build_common.hpp"
#include <dds/error/errors.hpp>
#include <dds/error/nonesuch.hpp>
#include <dds/util/log.hpp>

#include <boost/leaf/handle_exception.hpp>
#include <fansi/styled.hpp>

using namespace fansi::literals;

namespace dds::cli::cmd {

int compile_file(const options& opts) {
    return boost::leaf::try_catch(
        [&] {
            auto builder = create_project_builder(opts);
            builder.compile_files(opts.compile_file.files,
                                  {
                                      .out_root
                                      = opts.out_path.value_or(fs::current_path() / "_build"),
                                      .existing_lm_index = opts.build.lm_index,
                                      .emit_lmi          = {},
                                      .tweaks_dir        = opts.build.tweaks_dir,
                                      .toolchain         = opts.load_toolchain(),
                                      .parallel_jobs     = opts.jobs,
                                  });
            return 0;
        },
        [&](boost::leaf::catch_<dds::user_error<dds::errc::compile_failure>>,
            std::vector<e_nonesuch> missing) {
            if (missing.size() == 1) {
                dds_log(
                    error,
                    "Requested source file [.bold.red[{}]] is not a file compiled for this project"_styled,
                    missing.front().given);
            } else {
                dds_log(error, "The following files are not compiled as part of this project:");
                for (auto&& f : missing) {
                    dds_log(error, "  - .bold.red[{}]"_styled, f.given);
                }
            }
            write_error_marker("nonesuch-compile-file");
            return 2;
        },
        [&](boost::leaf::catch_<dds::user_error<dds::errc::compile_failure>> e) {
            dds_log(error, e.value().what());
            dds_log(error, "  (Refer to compiler output for more information)");
            write_error_marker("compile-file-failed");
            return 2;
        });
}

}  // namespace dds::cli::cmd
