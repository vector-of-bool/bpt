#pragma once

#include <dds/util/log.hpp>
#include <debate/argument_parser.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dds {

namespace fs = std::filesystem;
class pkg_db;
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
    sdist,
    repoman,
};

/**
 * @brief 'dds sdist' subcommands
 */
enum class sdist_subcommand {
    _none_,
    create,
};

/**
 * @brief 'dds pkg' subcommands
 */
enum class pkg_subcommand {
    _none_,
    ls,
    get,
    import,
    repo,
};

/**
 * @brief 'dds pkg repo' subcommands
 */
enum class cli_pkg_repo_subcommand {
    _none_,
    add,
    update,
};

/**
 * @brief 'dds repoman' subcommands
 *
 */
enum class repoman_subcommand {
    _none_,
    init,
    import,
    remove,
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

/**
 * @brief Complete aggregate of all dds command-line options, and some utilities
 */
struct options {
    using path       = fs::path;
    using opt_path   = std::optional<fs::path>;
    using string     = std::string;
    using opt_string = std::optional<std::string>;

    // The `--data-dir` argument
    opt_path data_dir;
    // The `--pkg-cache-dir' argument
    opt_path pkg_cache_dir;
    // The `--pkg-db-dir` argument
    opt_path pkg_db_dir;
    // The `--log-level` argument
    log::level log_level = log::level::info;

    // The top-most selected subcommand
    enum subcommand subcommand;

    // Many subcommands use a '--project' argument, stored here, using the CWD as the default
    path project_dir = fs::current_path();

    // Compile and build commands with `--no-warnings`/`--no-warn`
    bool disable_warnings = true;
    // Compile and build commands' `--jobs` parameter
    int jobs = 0;
    // Compile and build commands' `--toolchain` option:
    opt_string toolchain;
    opt_path   out_path;

    // Shared `--if-exists` argument:
    cli::if_exists if_exists = cli::if_exists::fail;

    /**
     * @brief Open the package pkg_db based on the user-specified options.
     * @return pkg_db
     */
    pkg_db open_catalog() const;
    /**
     * @brief Load a dds toolchain as specified by the user, or a default.
     * @return dds::toolchain
     */
    dds::toolchain load_toolchain() const;

    /**
     * @brief Parameters specific to 'dds build'
     */
    struct {
        bool                want_tests = true;
        bool                want_apps  = true;
        opt_path            lm_index;
        std::vector<string> add_repos;
        bool                update_repos = false;
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
    } build_deps;

    /**
     * @brief Parameters and subcommands for 'dds pkg'
     *
     */
    struct {
        /// The 'dds pkg' subcommand
        pkg_subcommand subcommand;

        /**
         * @brief Parameters for 'dds pkg import'
         */
        struct {
            /// File paths or URLs of packages to import
            std::vector<string> items;
            /// Allow piping a package tarball in through stdin
            bool from_stdin = false;
        } import;

        /**
         * @brief Parameters for 'dds pkg repo'
         */
        struct {
            /// The 'pkg repo' subcommand
            cli_pkg_repo_subcommand subcommand;

            /**
             * @brief Parameters of 'dds pkg repo add'
             */
            struct {
                /// The repository URL
                string url;
                /// Whether we should update repo data after adding the repository
                bool update = true;
            } add;
        } repo;

        /**
         * @brief Paramters for 'dds pkg get'
         */
        struct {
            /// Package IDs to download
            std::vector<string> pkgs;
        } get;
    } pkg;

    struct {
        sdist_subcommand subcommand;
    } sdist;

    /**
     * @brief Parameters for 'dds repoman'
     */
    struct {
        /// Shared parameter between repoman subcommands: The directory we are acting upon
        path repo_dir;

        /// The actual operation we are performing on the repository dir
        repoman_subcommand subcommand;

        /// Options for 'dds repoman init'
        struct {
            /// The name of the new repository. If not provided, a random one will be generated
            opt_string name;
        } init;

        /// Options for 'dds repoman import'
        struct {
            /// sdist tarball file paths to import into the repository
            std::vector<fs::path> files;
        } import;

        /// Options for 'dds repoman remove'
        struct {
            /// Package IDs of packages to remove
            std::vector<string> pkgs;
        } remove;
    } repoman;

    /**
     * @brief Attach arguments and subcommands to the given argument parser, binding those arguments
     * to the values in this object.
     */
    void setup_parser(debate::argument_parser& parser) noexcept;
};

}  // namespace cli
}  // namespace dds
