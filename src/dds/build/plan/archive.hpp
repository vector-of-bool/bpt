#pragma once

#include <dds/build/plan/compile_file.hpp>
#include <dds/util/fs.hpp>

#include <string>
#include <string_view>

namespace dds {

/**
 * Represents the intention to create an library archive. This also contains
 * the compile plans for individual files.
 *
 * This is distinct from `library_plan`, becuase this corresponds to an actual
 * static library and its compiled source files.
 */
class create_archive_plan {
    /// The name of the archive. Not the filename, but the base name thereof
    std::string _name;
    /// The qualified name of the library, as it would appear in a libman-usage
    std::string _qual_name;
    /// The subdirectory in which the archive should be generated.
    fs::path _subdir;
    /// The plans for compiling the constituent source files of this library
    std::vector<compile_file_plan> _compile_files;

public:
    /**
     * Construct an archive plan.
     * @param name The name of the archive
     * @param subdir The subdirectory in which the archive and its object files
     *      will be placed
     * @param cfs The file compilation plans that will be collected together to
     *      form the static library.
     */
    create_archive_plan(std::string_view               name,
                        std::string_view               qual_name,
                        path_ref                       subdir,
                        std::vector<compile_file_plan> cfs)
        : _name(name)
        , _qual_name(qual_name)
        , _subdir(subdir)
        , _compile_files(std::move(cfs)) {}

    /**
     * Get the name of the archive library.
     */
    const std::string& name() const noexcept { return _name; }

    /**
     * Calculate the path relative to the build output root where the static library archive will be
     * placed upon creation.
     * @param tc The toolchain that will be used
     */
    fs::path calc_archive_file_path(const toolchain& tc) const noexcept;

    /**
     * Get the compilation plans for this library.
     */
    auto& compile_files() const noexcept { return _compile_files; }

    /**
     * Perform the actual archive generation. Expects all compilations to have
     * completed.
     * @param env The build environment for the archival.
     */
    void archive(build_env_ref env) const;
};

}  // namespace dds
