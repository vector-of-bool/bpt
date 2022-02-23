#pragma once

#include <dds/util/log.hpp>
#include <debate/argument_parser.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dds {

namespace fs = std::filesystem;
class toolchain;

namespace cli {

/**
 * @brief Top-level dds subcommands
 */
enum class subcommand {
    _none_,
    build,
    compile_file,
    build_deps,
    pkg,
    repo,
    install_yourself,
};

/**
 * @brief 'dds pkg' subcommands
 */
enum class pkg_subcommand {
    _none_,
    create,
    search,
    prefetch,
    solve,
};

/**
 * @brief 'dds pkg repo' subcommands
 */
enum class pkg_repo_subcommand {
    _none_,
    add,
    remove,
    update,
    ls,
};

/**
 * @brief 'dds repo' subcommands
 *
 */
enum class repo_subcommand {
    _none_,
    init,
    import,
    remove,
    validate,
    ls,
};

/**
 * @brief Options for `--if-exists` on the CLI
 */
enum class if_exists {
    replace,
    fail,
    ignore,
};

enum class if_missing {
    fail,
    ignore,
};

enum class repo_sync_mode {
    always,
    cached_okay,
    never,
};

/**
 * @brief Complete aggregate of all dds command-line options, and some utilities
 */
struct options {
    using path       = fs::path;
    using opt_path   = std::optional<fs::path>;
    using string     = std::string;
    using opt_string = std::optional<std::string>;

    // The `--crs-cache-dir` argument
    opt_path crs_cache_dir;
    // The `--log-level` argument
    log::level log_level = log::level::info;
    // Any `--dry-run` argument
    bool dry_run = false;
    // A `--repo-sync-mode` argument
    cli::repo_sync_mode repo_sync_mode = cli::repo_sync_mode::always;

    // All `--use-repo` arguments
    std::vector<std::string> use_repos;
    // Toggle on/off the default repository
    bool use_default_repo = true;

    // The top-most selected subcommand
    enum subcommand subcommand;

    // Many subcommands use a '--project' argument, stored here, using the CWD as the default
    path project_dir = fs::current_path();

    // Compile and build commands with `--no-warnings`/`--no-warn`
    bool disable_warnings = false;
    // Compile and build commands' `--jobs` parameter
    int jobs = 0;
    // Compile and build commands' `--toolchain` option:
    opt_string toolchain;
    opt_path   out_path;

    // Shared `--if-exists` argument:
    cli::if_exists if_exists = cli::if_exists::fail;
    // Shared '--if-missing' argument:
    cli::if_missing if_missing = cli::if_missing::fail;

    /**
     * @brief Load a dds toolchain as specified by the user, or a default.
     * @return dds::toolchain
     */
    dds::toolchain load_toolchain() const;

    /**
     * @brief Parameters specific to 'dds build'
     */
    struct {
        bool     want_tests = true;
        bool     want_apps  = true;
        opt_path lm_index;
        opt_path tweaks_dir;
    } build;

    /**
     * @brief Parameters specific to 'dds compile-file'
     */
    struct {
        /// The files that the user has requested to be compiled
        std::vector<fs::path> files;
    } compile_file;

    /**
     * @brief Parameters specific to 'dds build-deps'
     */
    struct {
        /// Files listed with '--deps-file'
        std::vector<fs::path> deps_files;
        /// Dependency strings provided directly in the command-line
        std::vector<string> deps;
        /// Path to a CMake import file to write
        opt_path cmake_file;
    } build_deps;

    /**
     * @brief Parameters and subcommands for 'dds pkg'
     *
     */
    struct {
        /// The 'dds pkg' subcommand
        pkg_subcommand subcommand;

        /**
         * @brief Paramters for 'dds pkg prefetch'
         */
        struct {
            /// Package IDs to download
            std::vector<string> pkgs;
        } prefetch;

        /**
         * @brief Parameters for 'dds pkg search'
         */
        struct {
            /// The search pattern, if provided
            opt_string pattern;
        } search;

        /**
         * @brief Paramters for 'dds pkg solve'
         */
        struct {
            /// Requirements listed to solve
            std::vector<string> reqs;
        } solve;
    } pkg;

    /**
     * @brief Parameters for 'dds repo'
     */
    struct {
        /// Shared parameter between repo subcommands: The directory we are acting upon
        path repo_dir;

        /// The actual operation we are performing on the repository dir
        repo_subcommand subcommand;

        /// Options for 'dds repo init'
        struct {
            /// The name of the new repository. If not provided, a random one will be generated
            string name;
        } init;

        /// Options for 'dds repo import'
        struct {
            /// sdist tarball file paths to import into the repository
            std::vector<fs::path> files;
        } import;

        /// Options for 'dds repo remove'
        struct {
            /// Package IDs of packages to remove
            std::vector<string> pkgs;
        } remove;
    } repo;

    struct {
        enum where_e {
            system,
            user,
        } where
            = user;
        bool fixup_path_env = true;
        bool symlink        = false;
    } install_yourself;

    /**
     * @brief Attach arguments and subcommands to the given argument parser, binding those arguments
     * to the values in this object.
     */
    void setup_parser(debate::argument_parser& parser) noexcept;
};

}  // namespace cli
}  // namespace dds
