#include "./options.hpp"

#include <bpt/crs/cache.hpp>
#include <bpt/error/errors.hpp>
#include <bpt/error/on_error.hpp>
#include <bpt/error/toolchain.hpp>
#include <bpt/toolchain/errors.hpp>
#include <bpt/toolchain/from_json.hpp>
#include <bpt/toolchain/toolchain.hpp>
#include <bpt/util/env.hpp>
#include <bpt/util/fs/io.hpp>

#include <debate/enum.hpp>
#include <fansi/styled.hpp>

using namespace bpt;
using namespace debate;
using namespace fansi::literals;

namespace {

struct setup {
    bpt::cli::options& opts;

    explicit setup(bpt::cli::options& opts)
        : opts(opts) {}

    // Util argument common to a lot of operations
    argument if_exists_arg{
        .long_spellings = {"if-exists"},
        .help           = "What to do if the resource already exists",
        .valname        = "{replace,ignore,fail}",
        .action         = put_into(opts.if_exists),
    };

    argument if_missing_arg{
        .long_spellings = {"if-missing"},
        .help           = "What to do if the resource does not exist",
        .valname        = "{fail,ignore}",
        .action         = put_into(opts.if_missing),
    };

    argument toolchain_arg{
        .long_spellings  = {"toolchain"},
        .short_spellings = {"t"},
        .help            = "The toolchain to use when building",
        .valname         = "<file-or-id>",
        .action          = put_into(opts.toolchain),
    };

    argument project_arg{
        .long_spellings  = {"project"},
        .short_spellings = {"p"},
        .help            = "The project to build. If not given, uses the current working directory",
        .valname         = "<project-path>",
        .action          = put_into(opts.project_dir),
    };

    argument no_warn_arg{
        .long_spellings = {"no-warn", "no-warnings"},
        .help           = "Disable build warnings",
        .nargs          = 0,
        .action         = store_true(opts.disable_warnings),
    };

    argument out_arg{
        .long_spellings  = {"out", "output"},
        .short_spellings = {"o"},
        .help            = "Path to the output",
        .valname         = "<path>",
        .action          = put_into(opts.out_path),
    };

    argument jobs_arg{
        .long_spellings  = {"jobs"},
        .short_spellings = {"j"},
        .help            = "Set the maximum number of parallel jobs to execute",
        .valname         = "<job-count>",
        .action          = put_into(opts.jobs),
    };

    argument repo_repo_dir_arg{
        .help     = "The directory of the repository to manage",
        .valname  = "<repo-dir>",
        .required = true,
        .action   = put_into(opts.repo.repo_dir),
    };

    argument tweaks_dir_arg{
        .long_spellings  = {"tweaks-dir"},
        .short_spellings = {"TD"},
        .help
        = "Base directory of "
          "\x1b]8;;https://vector-of-bool.github.io/2020/10/04/lib-configuration.html\x1b\\tweak "
          "headers\x1b]8;;\x1b\\ that should be available to the build.",
        .valname = "<dir>",
        .action  = put_into(opts.build.tweaks_dir),
    };

    argument use_repos_arg{
        .long_spellings  = {"use-repo"},
        .short_spellings = {"r"},
        .help            = "Use the given repository when resolving dependencies",
        .valname         = "<repo-url-or-domain>",
        .can_repeat      = true,
        .action          = push_back_onto(opts.use_repos),
    };

    argument no_default_repo_arg{
        .long_spellings  = {"no-default-repo"},
        .short_spellings = {"NDR"},
        .help            = "Do not consult the default package repository [repo-3.bpt.pizza]",
        .nargs           = 0,
        .action          = store_false(opts.use_default_repo),
    };

    argument repo_sync_arg{
        .long_spellings = {"repo-sync"},
        .help
        = "Mode for repository synchronization. Default is 'always'.\n"
          "\n"
          "always:\n  Attempt to pull repository metadata. If that fails, fail immediately and "
          "unconditionally.\n\n"
          "cached-okay:\n  Attempt to pull repository metadata. If a low-level network error "
          "occurs and \n  we have cached metadata for the failing repitory, ignore the error and "
          "continue.\n\n"
          "never:\n  Do not attempt to pull repository metadata. This option requires that there "
          "be a local cache of the repository metadata.",
        .valname = "{always,cached-okay,never}",
        .action  = put_into(opts.repo_sync_mode),
    };

    void do_setup(argument_parser& parser) noexcept {
        parser.add_argument({
            .long_spellings  = {"log-level"},
            .short_spellings = {"l"},
            .help            = "Set the bpt logging level. One of 'trace', 'debug', 'info', \n"
                               "'warn', 'error', 'critical', or 'silent'",
            .valname         = "<level>",
            .action          = put_into(opts.log_level),
        });
        parser.add_argument({
            .long_spellings = {"crs-cache-dir"},
            .help           = "(Advanced) Override bpt's CRS caching directory.",
            .valname        = "<directory>",
            .action         = put_into(opts.crs_cache_dir),
        });

        setup_main_commands(parser.add_subparsers({
            .description = "The operation to perform",
            .action      = put_into(opts.subcommand),
        }));
    }

    void setup_main_commands(subparser_group& group) {
        setup_build_cmd(group.add_parser({
            .name = "build",
            .help = "Build a project",
        }));
        setup_compile_file_cmd(group.add_parser({
            .name = "compile-file",
            .help = "Compile individual files in the project",
        }));
        setup_build_deps_cmd(group.add_parser({
            .name = "build-deps",
            .help = "Build a set of dependencies and generate a libman index",
        }));
        setup_pkg_cmd(group.add_parser({
            .name = "pkg",
            .help = "Manage packages and package remotes",
        }));
        setup_repo_cmd(group.add_parser({
            .name = "repo",
            .help = "Manage a CRS package repository",
        }));
        setup_install_yourself_cmd(group.add_parser({
            .name = "install-yourself",
            .help = "Have this bpt executable install itself onto your PATH",
        }));
        setup_new_cmd(group.add_parser({
            .name = "new",
            .help = "Generate a new bpt project",
        }));
    }

    void add_repo_args(argument_parser& cmd) {
        cmd.add_argument(use_repos_arg.dup());
        cmd.add_argument(no_default_repo_arg.dup());
        cmd.add_argument(repo_sync_arg.dup());
    }

    void setup_build_cmd(argument_parser& build_cmd) {
        build_cmd.add_argument(toolchain_arg.dup());
        build_cmd.add_argument(project_arg.dup());
        add_repo_args(build_cmd);
        build_cmd.add_argument({
            .long_spellings = {"no-tests"},
            .help           = "Do not build and run project tests",
            .nargs          = 0,
            .action         = debate::store_false(opts.build.want_tests),
        });
        build_cmd.add_argument({
            .long_spellings = {"no-apps"},
            .help           = "Do not build project applications",
            .nargs          = 0,
            .action         = debate::store_false(opts.build.want_apps),
        });
        build_cmd.add_argument(no_warn_arg.dup());
        build_cmd.add_argument(out_arg.dup()).help = "Directory where bpt will write build results";

        build_cmd.add_argument(jobs_arg.dup());
        build_cmd.add_argument(tweaks_dir_arg.dup());
    }

    void setup_compile_file_cmd(argument_parser& compile_file_cmd) noexcept {
        compile_file_cmd.add_argument(project_arg.dup());
        compile_file_cmd.add_argument(toolchain_arg.dup());
        compile_file_cmd.add_argument(no_warn_arg.dup()).help = "Disable compiler warnings";
        compile_file_cmd.add_argument(jobs_arg.dup()).help
            = "Set the maximum number of files to compile in parallel";
        compile_file_cmd.add_argument(out_arg.dup());
        compile_file_cmd.add_argument(tweaks_dir_arg.dup());
        add_repo_args(compile_file_cmd);
        compile_file_cmd.add_argument({
            .help       = "One or more source files to compile",
            .valname    = "<source-files>",
            .can_repeat = true,
            .action     = debate::push_back_onto(opts.compile_file.files),
        });
    }

    void setup_build_deps_cmd(argument_parser& build_deps_cmd) noexcept {
        build_deps_cmd.add_argument(toolchain_arg.dup()).required;
        build_deps_cmd.add_argument(jobs_arg.dup());
        build_deps_cmd.add_argument(out_arg.dup());
        build_deps_cmd.add_argument({
            .long_spellings = {"built-json"},
            .help           = "Destination of the generated '_built.json' file.",
            .valname        = "<lmi-path>",
            .action         = put_into(opts.build.built_json),
        });
        add_repo_args(build_deps_cmd);
        build_deps_cmd.add_argument({
            .long_spellings  = {"deps-file"},
            .short_spellings = {"d"},
            .help            = "Path to a YAML file listing dependencies",
            .valname         = "<deps-file>",
            .can_repeat      = true,
            .action          = debate::push_back_onto(opts.build_deps.deps_files),
        });
        build_deps_cmd.add_argument({
            .long_spellings = {"cmake"},
            .help = "Generate a CMake file at the given path that will create import targets for "
                    "the dependencies",
            .valname = "<file-path>",
            .action  = debate::put_into(opts.build_deps.cmake_file),
        });
        build_deps_cmd.add_argument(tweaks_dir_arg.dup());
        build_deps_cmd.add_argument({
            .help       = "Dependency statement strings",
            .valname    = "<dependency>",
            .can_repeat = true,
            .action     = debate::push_back_onto(opts.build_deps.deps),
        });
    }

    void setup_pkg_cmd(argument_parser& pkg_cmd) {
        auto& pkg_group = pkg_cmd.add_subparsers({
            .valname = "<pkg-subcommand>",
            .action  = put_into(opts.pkg.subcommand),
        });
        setup_pkg_create_cmd(pkg_group.add_parser({
            .name = "create",
            .help = "Create a source distribution archive of a project",
        }));
        setup_pkg_search_cmd(pkg_group.add_parser({
            .name = "search",
            .help = "Search for packages available to download",
        }));
        setup_pkg_prefetch_cmd(pkg_group.add_parser({
            .name = "prefetch",
            .help = "Sync a remote repository into the local package listing cache",
        }));
        setup_pkg_solve_cmd(pkg_group.add_parser({
            .name = "solve",
            .help = "Generate a dependency solution for the given requirements",
        }));
    }

    void setup_pkg_create_cmd(argument_parser& pkg_create_cmd) {
        pkg_create_cmd.add_argument(project_arg.dup()).help
            = "Path to the project for which to create a source distribution.\n"
              "Default is the current working directory.";
        pkg_create_cmd.add_argument(out_arg.dup()).help
            = "Destination path for the source distribution archive";
        pkg_create_cmd.add_argument(if_exists_arg.dup()).help
            = "What to do if the destination names an existing file";
        pkg_create_cmd.add_argument({
            .long_spellings = {"revision"},
            .help   = "The revision number of the generated package (The CRS \"pkg-version\")",
            .action = put_into(opts.pkg.create.revision),
        });
    }

    void setup_pkg_search_cmd(argument_parser& pkg_search_cmd) noexcept {
        add_repo_args(pkg_search_cmd);
        pkg_search_cmd.add_argument({
            .help
            = "A name or glob-style pattern. Only matching packages will be returned. \n"
              "Searching is case-insensitive. Only the .italic[name] will be matched (not the \n"
              "version).\n\nIf this parameter is omitted, the search will return .italic[all] \n"
              "available packages."_styled,
            .valname = "<name-or-pattern>",
            .action  = put_into(opts.pkg.search.pattern),
        });
    }

    void setup_pkg_prefetch_cmd(argument_parser& pkg_prefetch_cmd) noexcept {
        add_repo_args(pkg_prefetch_cmd);
        pkg_prefetch_cmd.add_argument({
            .help       = "List of package IDs to prefetch",
            .valname    = "<pkg-id>",
            .can_repeat = true,
            .action     = push_back_onto(opts.pkg.prefetch.pkgs),
        });
    }

    void setup_pkg_solve_cmd(argument_parser& pkg_solve_cmd) noexcept {
        add_repo_args(pkg_solve_cmd);
        pkg_solve_cmd.add_argument({
            .help       = "List of package requirements to solve for",
            .valname    = "<requirement>",
            .required   = true,
            .can_repeat = true,
            .action     = push_back_onto(opts.pkg.solve.reqs),
        });
    }

    void setup_repo_cmd(argument_parser& repo_cmd) noexcept {
        auto& grp = repo_cmd.add_subparsers({
            .valname = "<repo-subcommand>",
            .action  = put_into(opts.repo.subcommand),
        });
        setup_repo_init_cmd(grp.add_parser({
            .name = "init",
            .help = "Initialize a directory as a new CRS repository",
        }));
        setup_repo_import_cmd(grp.add_parser({
            .name = "import",
            .help = "Import a directory or package into a CRS repository",
        }));
        setup_repo_remove_cmd(grp.add_parser({
            .name = "remove",
            .help = "Remove packages from a CRS repository",
        }));
        auto& ls_cmd = grp.add_parser({
            .name = "ls",
            .help = "List the packages in a local CRS repository",
        });
        ls_cmd.add_argument(repo_repo_dir_arg.dup());
        auto& validate_cmd = grp.add_parser({
            .name = "validate",
            .help = "Check that all repository packages are valid and resolvable",
        });
        validate_cmd.add_argument(repo_repo_dir_arg.dup());
    }

    void setup_repo_import_cmd(argument_parser& repo_import_cmd) {
        repo_import_cmd.add_argument(repo_repo_dir_arg.dup());
        repo_import_cmd.add_argument(if_exists_arg.dup()).help
            = "Behavior when the package already exists in the repository";
        repo_import_cmd.add_argument({
            .help       = "Paths of CRS directories to import",
            .valname    = "<crs-path>",
            .can_repeat = true,
            .action     = push_back_onto(opts.repo.import.files),
        });
    }

    void setup_repo_init_cmd(argument_parser& repn_init_cmd) {
        repn_init_cmd.add_argument(repo_repo_dir_arg.dup());
        repn_init_cmd.add_argument(if_exists_arg.dup()).help
            = "What to do if the directory exists and is already repository";
        repn_init_cmd.add_argument({
            .long_spellings  = {"name"},
            .short_spellings = {"n"},
            .help            = "Specifiy the name of the new repository",
            .valname         = "<name>",
            .required        = true,
            .action          = put_into(opts.repo.init.name),
        });
    }

    void setup_repo_remove_cmd(argument_parser& repo_remove_cmd) {
        repo_remove_cmd.add_argument(repo_repo_dir_arg.dup());
        repo_remove_cmd.add_argument({
            .help       = "One or more identifiers of packages to remove",
            .valname    = "<pkg-id>",
            .can_repeat = true,
            .action     = push_back_onto(opts.repo.remove.pkgs),
        });
    }

    void setup_install_yourself_cmd(argument_parser& install_yourself_cmd) {
        install_yourself_cmd.add_argument({
            .long_spellings = {"where"},
            .help    = "The scope of the installation. For .bold[system], installs in a global \n"
                       "directory for all users of the system. For .bold[user], installs in a \n"
                       "user-specific directory for executable binaries."_styled,
            .valname = "{user,system}",
            .action  = put_into(opts.install_yourself.where),
        });
        install_yourself_cmd.add_argument({
            .long_spellings = {"dry-run"},
            .help
            = "Do not actually perform any operations, but log what .italic[would] happen"_styled,
            .nargs  = 0,
            .action = store_true(opts.dry_run),
        });
        install_yourself_cmd.add_argument({
            .long_spellings = {"no-modify-path"},
            .help           = "Do not attempt to modify the PATH environment variable",
            .nargs          = 0,
            .action         = store_false(opts.install_yourself.fixup_path_env),
        });
        install_yourself_cmd.add_argument({
            .long_spellings = {"symlink"},
            .help  = "Create a symlink at the installed location to the existing 'bpt' executable\n"
                     "instead of copying the executable file",
            .nargs = 0,
            .action = store_true(opts.install_yourself.symlink),
        });
    }

    void setup_new_cmd(argument_parser& parser) {
        parser.add_argument({
            .help    = "Name for the new project",
            .valname = "<project-name>",
            .action  = put_into(opts.new_.name),
        });
        parser.add_argument({
            .long_spellings = {"dir"},
            .help           = "Directory in which the project will be generated",
            .valname        = "<project-directory>",
            .action         = put_into(opts.new_.directory),
        });
        parser.add_argument({
            .long_spellings = {"split-src-include"},
            .help           = "Whether to split the [src/] and [include/] directories",
            .valname        = "{true,false}",
            .action         = parse_bool_into(opts.new_.split_src_include),
        });
    }
};

}  // namespace

void cli::options::setup_parser(debate::argument_parser& parser) noexcept {
    setup{*this}.do_setup(parser);
}

toolchain bpt::cli::options::load_toolchain() const {
    if (!toolchain) {
        return bpt::toolchain::get_default();
    }
    // Convert the given string to a toolchain
    auto& tc_str = *toolchain;
    BPT_E_SCOPE(e_loading_toolchain{tc_str});
    if (tc_str.starts_with(":")) {
        auto default_tc = tc_str.substr(1);
        return bpt::toolchain::get_builtin(default_tc);
    } else {
        BPT_E_SCOPE(bpt::e_toolchain_filepath{tc_str});
        return parse_toolchain_json5(bpt::read_file(tc_str));
    }
}

fs::path bpt::cli::options::absolute_project_dir_path() const noexcept {
    return bpt::resolve_path_weak(project_dir);
}

bool cli::options::default_from_env(std::string key, bool def) noexcept {
    auto env = bpt::getenv(key);
    if (env.has_value()) {
        return is_truthy_string(*env);
    }
    return def;
}

std::string cli::options::default_from_env(std::string key, std::string def) noexcept {
    return bpt::getenv(key).value_or(def);
}

int cli::options::default_from_env(std::string key, int def) noexcept {
    auto env = bpt::getenv(key);
    if (!env.has_value()) {
        return def;
    }
    int        r       = 0;
    const auto dat     = env->data();
    const auto dat_end = dat + env->size();
    auto       res     = std::from_chars(dat, dat_end, r);
    if (res.ptr != dat_end) {
        return def;
    }
    return r;
}

cli::options::options() noexcept {
    crs_cache_dir = bpt::getenv("BPT_CRS_CACHE_DIR", [] { return crs::cache::default_path(); });

    auto ll = getenv("BPT_LOG_LEVEL");
    if (ll.has_value()) {
        auto llo = magic_enum::enum_cast<log::level>(*ll);
        if (llo.has_value()) {
            log_level = *llo;
        }
    }

    toolchain = getenv("BPT_TOOLCHAIN");
    auto out  = getenv("BPT_OUTPUT_PATH");
    if (out.has_value()) {
        out_path = *out;
    }
}
