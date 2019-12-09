#include <dds/build.hpp>
#include <dds/catalog/catalog.hpp>
#include <dds/catalog/get.hpp>
#include <dds/repo/repo.hpp>
#include <dds/source/dist.hpp>
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

struct repo_path_flag : path_flag {
    repo_path_flag(args::Group& grp)
        : path_flag{grp,
                    "dir",
                    "Path to the DDS repository directory",
                    {"repo-dir"},
                    dds::repository::default_local_path()} {}
};

struct catalog_path_flag : path_flag {
    catalog_path_flag(args::Group& cmd)
        : path_flag(cmd,
                    "catalog-path",
                    "Override the path to the catalog database",
                    {"catalog", 'c'},
                    dds::dds_data_dir() / "catalog.db") {}

    dds::catalog open() { return dds::catalog::open(Get()); }
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

    struct {
        cli_catalog&  parent;
        args::Command cmd{parent.cat_group, "create", "Create a catalog database"};
        common_flags  _common{cmd};

        catalog_path_flag cat_path{cmd};

        int run() {
            // Simply opening the DB will initialize the catalog
            cat_path.open();
            return 0;
        }
    } create{*this};

    struct {
        cli_catalog&  parent;
        args::Command cmd{parent.cat_group, "import", "Import entries into a catalog"};
        common_flags  _common{cmd};

        catalog_path_flag cat_path{cmd};
        args::ValueFlagList<std::string>
            json_paths{cmd,
                       "json",
                       "Import catalog entries from the given JSON files",
                       {"json", 'j'}};

        int run() {
            auto cat = cat_path.open();
            for (const auto& json_fpath : json_paths.Get()) {
                cat.import_json_file(json_fpath);
            }
            return 0;
        }
    } import{*this};

    struct {
        cli_catalog&  parent;
        args::Command cmd{parent.cat_group, "get", "Obtain an sdist from a catalog listing"};
        common_flags  _common{cmd};

        catalog_path_flag cat_path{cmd};

        path_flag out{cmd,
                      "out",
                      "The directory where the source distributions will be placed",
                      {"out-dir", 'o'},
                      dds::fs::current_path()};

        args::PositionalList<std::string> requirements{cmd,
                                                       "requirement",
                                                       "The package IDs to obtain"};

        int run() {
            auto cat = cat_path.open();
            for (const auto& req : requirements.Get()) {
                auto id   = dds::package_id::parse(req);
                auto info = cat.get(id);
                if (!info) {
                    throw std::runtime_error(
                        fmt::format("No package in the catalog matched the ID '{}'", req));
                }
                auto tsd      = dds::get_package_sdist(*info);
                auto out_path = out.Get();
                auto dest     = out_path / id.to_string();
                spdlog::info("Create sdist at {}", dest.string());
                dds::fs::remove_all(dest);
                dds::safe_rename(tsd.sdist.path, dest);
            }
            return 0;
        }
    } get{*this};

    struct {
        cli_catalog&  parent;
        args::Command cmd{parent.cat_group, "add", "Manually add an entry to the catalog database"};
        common_flags  _common{cmd};

        catalog_path_flag cat_path{cmd};

        args::Positional<std::string> pkg_id{cmd,
                                             "id",
                                             "The name@version ID of the package to add",
                                             args::Options::Required};

        string_flag auto_lib{cmd,
                             "auto-lib",
                             "Set the auto-library information for this package",
                             {"auto-lib"}};

        args::ValueFlagList<std::string> deps{cmd,
                                              "depends",
                                              "The dependencies of this package",
                                              {"depends", 'd'}};

        string_flag git_url{cmd, "git-url", "The Git url for the package", {"git-url"}};
        string_flag git_ref{cmd,
                            "git-ref",
                            "The Git ref to from which the source distribution should be created",
                            {"git-ref"}};

        int run() {
            auto ident = dds::package_id::parse(pkg_id.Get());

            std::vector<dds::dependency> deps;
            for (const auto& dep : this->deps.Get()) {
                auto dep_id = dds::package_id::parse(dep);
                assert(false && "TODO");
                // deps.push_back({dep_id.name, dep_id.version});
            }

            dds::package_info info{ident, std::move(deps), {}};

            if (git_url) {
                if (!git_ref) {
                    throw std::runtime_error(
                        "`--git-ref` must be specified when using `--git-url`");
                }
                auto git = dds::git_remote_listing{git_url.Get(), git_ref.Get(), std::nullopt};
                if (auto_lib) {
                    git.auto_lib = lm::split_usage_string(auto_lib.Get());
                }
                info.remote = std::move(git);
            }

            cat_path.open().store(info);
            return 0;
        }
    } add{*this};

    struct {
        cli_catalog&  parent;
        args::Command cmd{parent.cat_group, "list", "List the contents of the catalog"};

        catalog_path_flag cat_path{cmd};
        string_flag name{cmd, "name", "Only list packages with the given name", {"name", 'n'}};

        int run() {
            auto cat  = cat_path.open();
            auto pkgs = name ? cat.by_name(name.Get()) : cat.all();
            for (const dds::package_id& pk : pkgs) {
                std::cout << pk.to_string() << '\n';
            }
            return 0;
        }
    } list{*this};

    int run() {
        if (create.cmd) {
            return create.run();
        } else if (import.cmd) {
            return import.run();
        } else if (get.cmd) {
            return get.run();
        } else if (add.cmd) {
            return add.run();
        } else if (list.cmd) {
            return list.run();
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

    repo_path_flag where{cmd};

    args::Group repo_group{cmd, "Repo subcommands"};

    struct {
        cli_repo&     parent;
        args::Command cmd{parent.repo_group, "ls", "List repository contents"};
        common_flags  _common{cmd};

        int run() {
            auto list_contents = [&](dds::repository repo) {
                auto same_name = [](auto&& a, auto&& b) {
                    return a.manifest.pkg_id.name == b.manifest.pkg_id.name;
                };

                auto all         = repo.iter_sdists();
                auto grp_by_name = all                             //
                    | ranges::views::group_by(same_name)           //
                    | ranges::views::transform(ranges::to_vector)  //
                    | ranges::views::transform([](auto&& grp) {
                                       assert(grp.size() > 0);
                                       return std::pair(grp[0].manifest.pkg_id.name, grp);
                                   });

                for (const auto& [name, grp] : grp_by_name) {
                    spdlog::info("{}:", name);
                    for (const dds::sdist& sd : grp) {
                        spdlog::info("  - {}", sd.manifest.pkg_id.version.to_string());
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

        repo_path_flag repo_where{cmd};
        args::Flag     force{cmd,
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

    catalog_path_flag cat_path{cmd};
    repo_path_flag    repo_path{cmd};

    args::Flag     no_tests{cmd, "no-tests", "Do not build and run tests", {"no-tests"}};
    args::Flag     no_apps{cmd, "no-apps", "Do not compile and link applications", {"no-apps"}};
    args::Flag     no_warnings{cmd, "no-warings", "Disable build warnings", {"no-warnings"}};
    toolchain_flag tc_filepath{cmd};

    args::Flag export_{cmd, "export", "Generate a library export", {"export", 'E'}};

    path_flag
        lm_index{cmd,
                 "lm_index",
                 "Path to an existing libman index from which to load deps (usually INDEX.lmi)",
                 {"lm-index", 'I'}};

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
        params.build_tests     = !no_tests.Get();
        params.build_apps      = !no_apps.Get();
        params.enable_warnings = !no_warnings.Get();
        params.parallel_jobs   = num_jobs.Get();
        dds::package_manifest man;
        const auto            man_filepath = params.root / "package.dds";
        if (exists(man_filepath)) {
            man = dds::package_manifest::load_from_file(man_filepath);
        }
        if (lm_index) {
            params.existing_lm_index = lm_index.Get();
        } else {
            // Download and build dependencies
            // Build the dependencies
            auto cat          = cat_path.open();
            params.dep_sdists = dds::repository::with_repository(  //
                this->repo_path.Get(),
                dds::repo_flags::write_lock | dds::repo_flags::create_if_absent,
                [&](dds::repository repo) {
                    // Download dependencies
                    auto deps = repo.solve(man.dependencies, cat);
                    for (const dds::package_id& pk : deps) {
                        auto exists = !!repo.find(pk);
                        if (!exists) {
                            spdlog::info("Download dependency: {}", pk.to_string());
                            auto opt_pkg = cat.get(pk);
                            assert(opt_pkg);
                            auto tsd = dds::get_package_sdist(*opt_pkg);
                            repo.add_sdist(tsd.sdist, dds::if_exists::throw_exc);
                        }
                    }
                    return deps  //
                        | ranges::views::transform([&](auto& id) {
                               auto ptr = repo.find(id);
                               assert(ptr);
                               return *ptr;
                           })
                        | ranges::to_vector;
                });
        }
        dds::build(params, man);
        return 0;
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
