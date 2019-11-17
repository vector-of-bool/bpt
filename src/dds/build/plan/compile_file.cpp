#include "./compile_file.hpp"

#include <dds/proc.hpp>
#include <dds/util/algo.hpp>
#include <dds/util/signal.hpp>
#include <dds/util/time.hpp>

#include <spdlog/spdlog.h>

#include <string>
#include <vector>

using namespace dds;

compile_command_info compile_file_plan::generate_compile_command(build_env_ref env) const noexcept {
    compile_file_spec spec{_source.path, calc_object_file_path(env)};
    spec.enable_warnings = _rules.enable_warnings();
    extend(spec.include_dirs, _rules.include_dirs());
    extend(spec.definitions, _rules.defs());
    return env.toolchain.create_compile_command(spec);
}

fs::path compile_file_plan::calc_object_file_path(const build_env& env) const noexcept {
    auto relpath = fs::relative(_source.path, _source.basis_path);
    auto ret     = env.output_root / _subdir / relpath;
    ret.replace_filename(relpath.filename().string() + env.toolchain.object_suffix());
    return fs::weakly_canonical(ret);
}
