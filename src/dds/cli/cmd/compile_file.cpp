#include "../options.hpp"

#include "./build_common.hpp"

namespace dds::cli::cmd {

int compile_file(const options& opts) {
    auto builder = create_project_builder(opts);
    builder.compile_files(opts.compile_file.files,
                          {
                              .out_root = opts.out_path.value_or(fs::current_path() / "_build"),
                              .existing_lm_index = opts.build.lm_index,
                              .emit_lmi          = {},
                              .toolchain         = opts.load_toolchain(),
                              .parallel_jobs     = opts.jobs,
                          });
    return 0;
}

}  // namespace dds::cli::cmd
