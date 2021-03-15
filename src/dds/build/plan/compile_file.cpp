#include "./compile_file.hpp"

#include <dds/proc.hpp>
#include <dds/util/algo.hpp>
#include <dds/util/signal.hpp>
#include <dds/util/time.hpp>

#include <string>
#include <vector>

using namespace dds;

compile_command_info compile_file_plan::generate_compile_command(build_env_ref env) const {
    compile_file_spec spec{_source.path, calc_object_file_path(env)};
    spec.enable_warnings = _rules.enable_warnings();
    for (auto dirpath : _rules.include_dirs()) {
        if (!dirpath.is_absolute()) {
            dirpath = env.output_root / dirpath;
        }
        dirpath = fs::weakly_canonical(dirpath);
        spec.include_dirs.push_back(std::move(dirpath));
    }
    for (const auto& use : _rules.uses()) {
        extend(spec.external_include_dirs, env.ureqs.include_paths(use));
    }
    extend(spec.definitions, _rules.defs());
    // Avoid huge command lines by shrinking down the list of #include dirs
    sort_unique_erase(spec.external_include_dirs);
    sort_unique_erase(spec.include_dirs);
    return env.toolchain.create_compile_command(spec, dds::fs::current_path(), env.knobs);
}

fs::path compile_file_plan::calc_object_file_path(const build_env& env) const noexcept {
    auto relpath = _source.relative_path();
    // The full output directory is prefixed by `_subdir`
    auto ret = env.output_root / _subdir / relpath;
    ret.replace_filename(relpath.filename().string() + env.toolchain.object_suffix());
    return fs::weakly_canonical(ret);
}
