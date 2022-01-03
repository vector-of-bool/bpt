#include "../options.hpp"

#include "./build_common.hpp"

#include <dds/build/builder.hpp>
#include <dds/error/errors.hpp>
#include <dds/toolchain/from_json.hpp>

using namespace dds;

namespace dds::cli::cmd {

static int _build(const options& opts) {
    auto builder = create_project_builder(opts);
    builder.build({
        .out_root          = opts.out_path.value_or(fs::current_path() / "_build"),
        .existing_lm_index = opts.build.lm_index,
        .emit_lmi          = {},
        .tweaks_dir        = opts.build.tweaks_dir,
        .toolchain         = opts.load_toolchain(),
        .parallel_jobs     = opts.jobs,
    });

    return 0;
}

int build(const options& opts) {
    return handle_build_error([&] { return _build(opts); });
}

}  // namespace dds::cli::cmd
