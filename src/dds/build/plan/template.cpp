#include <dds/build/plan/template.hpp>

#include <dds/util/fs.hpp>

using namespace dds;

void render_template_plan::render(build_env_ref env) const {
    auto content = slurp_file(_source.path);

    // Calculate the destination of the template rendering
    auto dest = env.output_root / _subdir / _source.relative_path();
    dest.replace_filename(dest.stem().stem().filename().string() + dest.extension().string());
    fs::create_directories(dest.parent_path());

    fs::copy_file(_source.path, dest, fs::copy_options::overwrite_existing);
}
