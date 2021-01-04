#pragma once

#include <dds/build/plan/base.hpp>
#include <dds/sdist/file.hpp>

#include <utility>

namespace dds {

class library_root;

class render_template_plan {
    /**
     * The source file that defines the config template
     */
    source_file _source;
    /**
     * The subdirectory in which the template should be rendered.
     */
    fs::path _subdir;

public:
    /**
     * Create a new instance
     * @param sf The source file of the template
     * @param subdir The subdirectort into which the template should render
     */
    render_template_plan(source_file sf, path_ref subdir)
        : _source(std::move(sf))
        , _subdir(subdir) {}

    /**
     * Render the template into its output directory
     */
    void render(build_env_ref, const library_root& owning_library) const;
};

}  // namespace dds