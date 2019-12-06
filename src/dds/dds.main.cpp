#include <dds/build.hpp>
#include <dds/catalog/catalog.hpp>
#include <dds/catalog/get.hpp>
#include <dds/repo/remote.hpp>
#include <dds/repo/repo.hpp>
#include <dds/sdist.hpp>
#include <dds/toolchain/from_dds.hpp>
#include <dds/util/fs.hpp>
#include <dds/util/paths.hpp>
#include <dds/util/signal.hpp>

#include <range/v3/view/group_by.hpp>
#include <range/v3/view/transform.hpp>

#include <dds/3rd/args.hxx>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <iostream>

namespace {

using string_flag = args::ValueFlag<std::string>;
using path_flag   = args::ValueFlag<dds::fs::path>;

struct toolchain_flag : string_flag {
    toolchain_flag(args::Group& grp)
        : string_flag{grp,
                      "toolchain_file",
                      "Path/ident of the toolchain to use",
                      {"toolchain", 't'},
                      (dds::fs::current_path() / "toolchain.dds").string()} {}

    dds::toolchain get_toolchain() {
        const auto tc_path = this->Get();
        if (tc_path.find(":") == 0) {
            auto default_tc = tc_path.substr(1);
            auto tc         = dds::toolchain::get_builtin(default_tc);
            if (!tc.has_value()) {
                throw std::runtime_error(
                    fmt::format("Invalid default toolchain name '{}'", default_tc));
            }
            return std::move(*tc);
        } else {
            return dds::parse_toolchain_dds(dds::slurp_file(tc_path));
        }
    }
};

struct repo_where_flag : path_flag {
    repo_where_flag(args::Group& grp)
        : path_flag{grp,
                    "dir",
                    "Path to the DDS repository directory",
                    {"repo-dir"},
                    dds::repository::default_local_path()} {}
};

/**
 * Base class holds the actual argument parser
 */
struct cli_base {
    args::ArgumentParser& parser;
    args::HelpFlag _help{parser, "help", "Display this help message and exit", {'h', "help"}};

    // Test argument:
    args::Flag _verify_ident{parser,
                             "test",
                             "Print `yes` and exit 0. Useful for scripting.",
                             {"are-you-the-real-dds?"}};

    args::Group cmd_group{parser, "Available Commands"};
};

/**
 * Flags common to all subcommands
 */
struct common_flags {
    args::Command& cmd;

    args::HelpFlag _help{cmd, "help", "Print this help message and exit", {'h', "help"}};
};

/**
 * Flags common to project-related commands
 */
struct common_project_flags {
    args::Command& cmd;

    path_flag root{cmd,
                   "project_dir",
                   "Path to the directory containing the project",
                   {'p', "project-dir"},
                   dds::fs::current_path()};
};

/*
 ######     ###    ########    ###    ##        #######   ######
##    ##   ## ##      ##      ## ##   ##       ##     ## ##    ##
##        ##   ##     ##     ##   ##  ##       ##     ## ##
##       ##     ##    ##    ##     ## ##       ##     ## ##   ####
##       #########    ##    ######### ##       ##     ## ##    ##
##    ## ##     ##    ##    ##     ## ##       ##     ## ##    ##
 ######  ##     ##    ##    ##     ## ########  #######   ######
*/

struct cli_catalog {
    cli_base&     base;
    args::Command cmd{base.cmd_group, "catalog", "Manage the package catalog"};
    common_flags  _common{cmd};

    args::Group cat_group{cmd, "Catalog subcommands"};

    struct catalog_path_flag : path_flag {
        catalog_path_flag(args::Group& cmd)
            : path_flag(cmd,
                        "catalog-path",
                        "Override the path to the catalog database",
                        {"catalog", 'c'},
                        dds::dds_data_dir() / "catalog.db") {}
    };

    struct {
        cli_catalog&  parent;
        args::Command cmd{parent.cat_group, "create", "Create a catalog database"};

        catalog_path_flag path{cmd};

        int run() {
            // Simply opening the DB will initialize the catalog
            dds::catalog::open(path.Get());
            return 0;
        }
    } create{*this};

    struct {
        cli_catalog&  parent;
        args::Command cmd{parent.cat_group, "import", "Import json data into a catalog"};

        catalog_path_flag                 path{cmd};
        args::PositionalList<std::string> json_paths{cmd,
                                                     "json",
                                                     "Path to the JSON files to import"};

        int run() {
            auto cat = dds::catalog::open(path.Get());
            for (const auto& json_fpath : json_paths.Get()) {
                cat.import_json_file(json_fpath);
            }
            return 0;
        }
    } import{*this};

    struct {
        cli_catalog&  parent;
        args::Command cmd{parent.cat_group, "get", "Obtain an sdist from a catalog listing"};

        catalog_path_flag                 path{cmd};
        args::PositionalList<std::string> requirements{cmd,
                                                       "requirement",
                                                       "The package IDs to obtain"};

        int run() {
            auto cat = dds::catalog::open(path.Get());
            for (const auto& req : requirements.Get()) {
                auto id   = dds::package_id::parse(req);
                auto info = cat.get(id);
                if (!info) {
                    throw std::runtime_error(
                        fmt::format("No package in the catalog matched the given ID"));
                }
                dds::get_package_sdist(*info);
            }
            return 0;
        }
    } get{*this};

    int run() {
        if (create.cmd) {
            return create.run();
        } else if (import.cmd) {
            return import.run();
        } else if (get.cmd) {
            return get.run();
        } else {
            assert(false);
            std::terminate();
        }
    }
};

/*
########  ######## ########   #######
##     ## ##       ##     ## ##     ##
##     ## ##       ##     ## ##     ##
########  ######   ########  ##     ##
##   ##   ##       ##        ##     ##
##    ##  ##       ##        ##     ##
##     ## ######## ##         #######
*/

struct cli_repo {
    cli_base&     base;
    args::Command cmd{base.cmd_group, "repo", "Manage the package repository"};
    common_flags  _common{cmd};

    repo_where_flag where{cmd};

    args::Group repo_group{cmd, "Repo subcommands"};

    struct {
        cli_repo&     parent;
        args::Command cmd{parent.repo_group, "ls", "List repository contents"};
        common_flags  _common{cmd};

        int run() {
            auto list_contents = [&](dds::repository repo) {
                auto same_name = [](auto&& a, auto&& b) {
                    return a.manifest.pk_id.name == b.manifest.pk_id.name;
                };

                auto all         = repo.iter_sdists();
                auto grp_by_name = all                             //
                    | ranges::views::group_by(same_name)           //
                    | ranges::views::transform(ranges::to_vector)  //
                    | ranges::views::transform([](auto&& grp) {
                                       assert(grp.size() > 0);
                                       return std::pair(grp[0].manifest.pk_id.name, grp);
                                   });

                for (const auto& [name, grp] : grp_by_name) {
                    spdlog::info("{}:", name);
                    for (const dds::sdist& sd : grp) {
                        spdlog::info("  - {}", sd.manifest.pk_id.version.to_string());
                    }
                }

                return 0;
            };
            return dds::repository::with_repository(parent.where.Get(),
                                                    dds::repo_flags::read,
                                                    list_contents);
        }
    } ls{*this};

    struct {
        cli_repo&     parent;
        args::Command cmd{parent.repo_group, "init", "Initialize a directory as a repository"};
        common_flags  _common{cmd};

        int run() {
            if (parent.where.Get().empty()) {
                throw args::ParseError("The --dir flag is required");
            }
            auto repo_dir = dds::fs::absolute(parent.where.Get());
            dds::repository::with_repository(repo_dir, dds::repo_flags::create_if_absent, [](auto) {
            });
            return 0;
        }
    } init{*this};

    int run() {
        if (ls.cmd) {
            return ls.run();
        } else if (init.cmd) {
            return init.run();
        } else {
            assert(false);
            std::terminate();
        }
    }
};

/*
 ######  ########  ####  ######  ########
##    ## ##     ##  ##  ##    ##    ##
##       ##     ##  ##  ##          ##
 ######  ##     ##  ##   ######     ##
      ## ##     ##  ##        ##    ##
##    ## ##     ##  ##  ##    ##    ##
 ######  ########  ####  ######     ##
*/

struct cli_sdist {
    cli_base&     base;
    args::Command cmd{base.cmd_group, "sdist", "Work with source distributions"};

    common_flags _common{cmd};

    args::Group sdist_group{cmd, "`sdist` commands"};

    struct {
        cli_sdist&    parent;
        args::Command cmd{parent.sdist_group, "create", "Create a source distribution"};

        common_project_flags project{cmd};

        path_flag out{cmd,
                      "out",
                      "The destination of the source distribution",
                      {"out"},
                      dds::fs::current_path() / "project.dsd"};

        args::Flag force{cmd,
                         "replace-if-exists",
                         "Forcibly replace an existing distribution",
                         {"replace"}};

        int run() {
            dds::sdist_params params;
            params.project_dir = project.root.Get();
            params.dest_path   = out.Get();
            params.force       = force.Get();
            dds::create_sdist(params);
            return 0;
        }
    } create{*this};

    struct {
        cli_sdist&    parent;
        args::Command cmd{parent.sdist_group,
                          "export",
                          "Export a source distribution to a repository"};

        common_project_flags project{cmd};

        repo_where_flag repo_where{cmd};
        args::Flag      force{cmd,
                         "replace-if-exists",
                         "Replace an existing export in the repository",
                         {"replace"}};

        int run() {
            auto repo_dir = repo_where.Get();
            // TODO: Generate a unique name to avoid conflicts
            auto tmp_sdist = dds::fs::temp_directory_path() / ".dds-sdist";
            if (dds::fs::exists(tmp_sdist)) {
                dds::fs::remove_all(tmp_sdist);
            }
            dds::sdist_params params;
            params.project_dir = project.root.Get();
            params.dest_path   = tmp_sdist;
            params.force       = true;
            auto sdist         = dds::create_sdist(params);
            dds::repository::with_repository(  //
                repo_dir,
                dds::repo_flags::create_if_absent | dds::repo_flags::write_lock,
                [&](dds::repository repo) {  //
                    repo.add_sdist(sdist,
                                   force.Get() ? dds::if_exists::replace
                                               : dds::if_exists::throw_exc);
                });
            return 0;
        }
    } export_{*this};

    int run() {
        if (create.cmd) {
            return create.run();
        } else if (export_.cmd) {
            return export_.run();
        } else {
            assert(false && "Unreachable");
            std::terminate();
        }
    }
};

/*
########  ##     ## #### ##       ########
##     ## ##     ##  ##  ##       ##     ##
##     ## ##     ##  ##  ##       ##     ##
########  ##     ##  ##  ##       ##     ##
##     ## ##     ##  ##  ##       ##     ##
##     ## ##     ##  ##  ##       ##     ##
########   #######  #### ######## ########
*/

struct cli_build {
    cli_base&     base;
    args::Command cmd{base.cmd_group, "build", "Build a project"};

    common_flags _common{cmd};

    common_project_flags project{cmd};

    args::Flag     build_tests{cmd, "build_tests", "Build and run the tests", {"tests", 'T'}};
    args::Flag     build_apps{cmd, "build_apps", "Build applications", {"apps", 'A'}};
    args::Flag     export_{cmd, "export", "Generate a library export", {"export", 'E'}};
    toolchain_flag tc_filepath{cmd};

    args::Flag enable_warnings{cmd,
                               "enable_warnings",
                               "Enable compiler warnings",
                               {"warnings", 'W'}};

    path_flag lm_index{cmd,
                       "lm_index",
                       "Path to a libman index (usually INDEX.lmi)",
                       {"lm-index", 'I'},
                       dds::fs::path()};

    args::ValueFlag<int> num_jobs{cmd,
                                  "jobs",
                                  "Set the number of parallel jobs when compiling files",
                                  {"jobs", 'j'},
                                  0};

    path_flag out{cmd,
                  "out",
                  "The root build directory",
                  {"out"},
                  dds::fs::current_path() / "_build"};

    int run() {
        dds::build_params params;
        params.root            = project.root.Get();
        params.out_root        = out.Get();
        params.toolchain       = tc_filepath.get_toolchain();
        params.do_export       = export_.Get();
        params.build_tests     = build_tests.Get();
        params.build_apps      = build_apps.Get();
        params.enable_warnings = enable_warnings.Get();
        params.parallel_jobs   = num_jobs.Get();
        params.lm_index        = lm_index.Get();
        dds::package_manifest man;
        const auto            man_filepath = params.root / "package.dds";
        if (exists(man_filepath)) {
            man = dds::package_manifest::load_from_file(man_filepath);
        }
        dds::build(params, man);
        return 0;
    }
};

/*
########  ######## ########   ######
##     ## ##       ##     ## ##    ##
##     ## ##       ##     ## ##
##     ## ######   ########   ######
##     ## ##       ##              ##
##     ## ##       ##        ##    ##
########  ######## ##         ######
*/

struct cli_deps {
    cli_base&     base;
    args::Command cmd{base.cmd_group, "deps", "Obtain/inspect/build deps for the project"};

    common_flags         _flags{cmd};
    common_project_flags project{cmd};

    args::Group deps_group{cmd, "Subcommands"};

    dds::package_manifest load_package_manifest() {
        return dds::package_manifest::load_from_file(project.root.Get() / "package.dds");
    }

    struct {
        cli_deps&     parent;
        args::Command cmd{parent.deps_group, "ls", "List project dependencies"};
        common_flags  _common{cmd};

        int run() {
            const auto man = parent.load_package_manifest();
            for (const auto& dep : man.dependencies) {
                std::cout << dep.name << " " << dep.version.to_string() << '\n';
            }
            return 0;
        }
    } ls{*this};

    struct {
        cli_deps&     parent;
        args::Command cmd{parent.deps_group,
                          "get",
                          "Ensure we have local copies of the project dependencies"};
        common_flags  _common{cmd};

        repo_where_flag repo_where{cmd};
        path_flag       remote_listing_file{
            cmd,
            "remote-listing",
            "Path to a file containing listing of remote sdists and how to obtain them",
            {'R', "remote-list"},
            "remote.dds"};

        int run() {
            auto man    = parent.load_package_manifest();
            auto rd     = dds::remote_directory::load_from_file(remote_listing_file.Get());
            bool failed = false;
            dds::repository::with_repository(  //
                repo_where.Get(),
                dds::repo_flags::write_lock | dds::repo_flags::create_if_absent,
                [&](dds::repository repo) {
                    for (auto& dep : man.dependencies) {
                        auto exists = !!repo.find(dep.name, dep.version);
                        if (!exists) {
                            spdlog::info("Pull remote: {} {}", dep.name, dep.version.to_string());
                            auto opt_remote = rd.find(dep.name, dep.version);
                            if (opt_remote) {
                                auto tsd = opt_remote->pull_sdist();
                                repo.add_sdist(tsd.sdist, dds::if_exists::ignore);
                            } else {
                                spdlog::error("No remote listing for {} {}",
                                              dep.name,
                                              dep.version.to_string());
                                failed = true;
                            }
                        } else {
                            spdlog::info("Okay: {} {}", dep.name, dep.version.to_string());
                        }
                    }
                });
            if (failed) {
                return 1;
            }
            return 0;
        }
    } get{*this};

    struct {
        cli_deps&     parent;
        args::Command cmd{parent.deps_group, "build", "Build project dependencies"};
        common_flags  _common{cmd};

        path_flag  build_dir{cmd,
                            "build_dir",
                            "Directory where build results will be stored",
                            {"deps-build-dir"},
                            dds::fs::current_path() / "_build/deps"};
        path_flag  lmi_path{cmd,
                           "lmi_path",
                           "Destination for the INDEX.lmi file",
                           {"lmi-path"},
                           dds::fs::current_path() / "_build/INDEX.lmi"};
        args::Flag no_lmi{cmd,
                          "no_lmi",
                          "If specified, will not generate an INDEX.lmi",
                          {"skip-lmi"}};

        repo_where_flag repo_where{cmd};

        toolchain_flag tc_filepath{cmd};

        int run() {
            auto man  = parent.load_package_manifest();
            auto deps = dds::repository::with_repository(  //
                repo_where.Get(),
                dds::repo_flags::read,
                [&](dds::repository repo) {
                    return find_dependencies(repo,
                                             man.dependencies.begin(),
                                             man.dependencies.end());
                });

            auto tc   = tc_filepath.get_toolchain();
            auto bdir = build_dir.Get();
            dds::fs::create_directories(bdir);
            auto           db = dds::database::open(bdir / ".dds.db");
            dds::build_env env{std::move(tc), bdir, db};

            auto plan = dds::create_deps_build_plan(deps, env);
            plan.compile_all(env, 6);
            plan.archive_all(env, 6);
            if (!no_lmi.Get()) {
                write_libman_index(lmi_path.Get(), plan, env);
            }
            return 0;
        }
    } build{*this};

    int run() {
        if (ls.cmd) {
            return ls.run();
        } else if (build.cmd) {
            return build.run();
        } else if (get.cmd) {
            return get.run();
        }
        std::terminate();
    }
};

}  // namespace

/*
##     ##    ###    #### ##    ##
###   ###   ## ##    ##  ###   ##
#### ####  ##   ##   ##  ####  ##
## ### ## ##     ##  ##  ## ## ##
##     ## #########  ##  ##  ####
##     ## ##     ##  ##  ##   ###
##     ## ##     ## #### ##    ##
*/

int main(int argc, char** argv) {
#if DDS_DEBUG
    spdlog::set_level(spdlog::level::debug);
#endif
    spdlog::set_pattern("[%H:%M:%S] [%^%-5l%$] %v");
    args::ArgumentParser parser("DDS - The drop-dead-simple library manager");

    cli_base    cli{parser};
    cli_build   build{cli};
    cli_sdist   sdist{cli};
    cli_repo    repo{cli};
    cli_deps    deps{cli};
    cli_catalog catalog{cli};
    try {
        parser.ParseCLI(argc, argv);
    } catch (const args::Help&) {
        std::cout << parser;
        return 0;
    } catch (const args::Error& e) {
        std::cerr << parser;
        std::cerr << e.what() << '\n';
        return 1;
    }

    dds::install_signal_handlers();

    try {
        if (cli._verify_ident) {
            std::cout << "yes\n";
            return 0;
        } else if (build.cmd) {
            return build.run();
        } else if (sdist.cmd) {
            return sdist.run();
        } else if (repo.cmd) {
            return repo.run();
        } else if (deps.cmd) {
            return deps.run();
        } else if (catalog.cmd) {
            return catalog.run();
        } else {
            assert(false);
            std::terminate();
        }
    } catch (const dds::user_cancelled&) {
        spdlog::critical("Operation cancelled by user");
        return 2;
    } catch (const std::exception& e) {
        spdlog::critical(e.what());
        return 2;
    }
}
