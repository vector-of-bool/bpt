#pragma once

#include <dds/build/plan/base.hpp>
#include <dds/sdist/file.hpp>

#include <libman/library.hpp>

#include <memory>

namespace dds {

/**
 * Exception thrown to indicate a compile failure
 */
struct compile_failure : std::runtime_error {
    using runtime_error::runtime_error;
};

/**
 * Because we may have many files in a library, we store base file compilation
 * parameters in a single object that implements shared semantics. Copying the
 * object is cheap, and updates propagate between all copies. Distinct copies
 * can be made via the `clone()` method.
 */
class shared_compile_file_rules {
    /// The attributes we track
    struct rules_impl {
        std::vector<fs::path>    inc_dirs;
        std::vector<std::string> defs;
        std::vector<lm::usage>   uses;
        bool                     enable_warnings = false;
    };

    /// The actual PIMPL.
    std::shared_ptr<rules_impl> _impl = std::make_shared<rules_impl>();

public:
    /**
     * Create a detached copy of these rules. Updates to the copy do not affect the original.
     */
    auto clone() const noexcept {
        auto cp  = *this;
        cp._impl = std::make_shared<rules_impl>(*_impl);
        return cp;
    }

    /**
     * Access the include directories for these rules
     */
    auto& include_dirs() noexcept { return _impl->inc_dirs; }
    auto& include_dirs() const noexcept { return _impl->inc_dirs; }

    /**
     * Access the preprocessor definitions for these rules
     */
    auto& defs() noexcept { return _impl->defs; }
    auto& defs() const noexcept { return _impl->defs; }

    /**
     * Access the named usage requirements for this set of rules
     */
    auto& uses() noexcept { return _impl->uses; }
    auto& uses() const noexcept { return _impl->uses; }

    /**
     * A boolean to toggle compile warnings for the associated compiles
     */
    auto& enable_warnings() noexcept { return _impl->enable_warnings; }
    auto& enable_warnings() const noexcept { return _impl->enable_warnings; }
};

/**
 * Represents the parameters to compile an individual file. This includes the
 * original source file path, and the shared compile rules as defined
 * by `shared_compile_file_rules`.
 */
class compile_file_plan {
    /// The shared rules
    shared_compile_file_rules _rules;
    /// The source file object that we are compiling
    source_file _source;
    /// A "qualifier" to be shown in log messages (not otherwise significant)
    std::string _qualifier;
    /// The subdirectory in which the object file will be generated
    fs::path _subdir;

public:
    /**
     * Create a new instance.
     * @param rules The base compile rules
     * @param sf The source file that will be compiled
     * @param qual An arbitrary qualifier for the source file, shown in log output
     * @param subdir The subdirectory where the object file will be generated
     */
    compile_file_plan(shared_compile_file_rules rules,
                      source_file               sf,
                      std::string_view          qual,
                      path_ref                  subdir)
        : _rules(rules)
        , _source(std::move(sf))
        , _qualifier(qual)
        , _subdir(subdir) {}

    /**
     * The `source_file` object for this plan.
     */
    const source_file& source() const noexcept { return _source; }
    /**
     * The path to the source file
     */
    path_ref source_path() const noexcept { return _source.path; }
    /**
     * The shared rules for this compilation
     */
    auto& rules() const noexcept { return _rules; }
    /**
     * The arbitrary qualifier for this compilation
     */
    auto& qualifier() const noexcept { return _qualifier; }

    /**
     * Generate the path that will be the destination of this compile output
     */
    fs::path calc_object_file_path(build_env_ref env) const noexcept;
    /**
     * Generate a concrete compile command object for this source file for the given build
     * environment.
     */
    compile_command_info generate_compile_command(build_env_ref) const;
};

}  // namespace dds