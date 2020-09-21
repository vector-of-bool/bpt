#include <dds/build/builder.hpp>
#include <dds/catalog/catalog.hpp>
#include <dds/catalog/get.hpp>
#include <dds/dym.hpp>
#include <dds/error/errors.hpp>
#include <dds/repo/repo.hpp>
#include <dds/source/dist.hpp>
#include <dds/toolchain/from_json.hpp>
#include <dds/util/fs.hpp>
#include <dds/util/log.hpp>
#include <dds/util/paths.hpp>
#include <dds/util/signal.hpp>

#include <neo/assert.hpp>
#include <range/v3/action/join.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/group_by.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include <dds/3rd/args.hxx>

#include <filesystem>
#include <iostream>
#include <locale.h>
#include <sstream>

namespace {

using string_flag = args::ValueFlag<std::string>;
using path_flag   = args::ValueFlag<dds::fs::path>;

struct toolchain_flag : string_flag {
    toolchain_flag(args::Group& grp)
        : string_flag{grp,
                      "toolchain_file",
                      "Path/identifier of the toolchain to use",
                      {"toolchain", 't'}} {}

    dds::toolchain get_toolchain() {
        if (*this) {
            return get_arg();
        } else {
            auto found = dds::toolchain::get_default();
            if (!found) {
                dds::throw_user_error<dds::errc::no_default_toolchain>();
            }
            return *found;
        }
    }

    dds::toolchain get_arg() {
        const auto tc_path = this->Get();
        if (tc_path.find(":") == 0) {
            auto default_tc = tc_path.substr(1);
            auto tc         = dds::toolchain::get_builtin(default_tc);
            if (!tc.has_value()) {
                dds::throw_user_error<
                    dds::errc::invalid_builtin_toolchain>("Invalid built-in toolchain name '{}'",
                                                          default_tc);
            }
            return std::move(*tc);
        } else {
            return dds::parse_toolchain_json5(dds::slurp_file(tc_path));
            // return dds::parse_toolchain_dds(dds::slurp_file(tc_path));
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

struct num_jobs_flag : args::ValueFlag<int> {
    num_jobs_flag(args::Group& cmd)
        : ValueFlag(cmd,
                    "jobs",
                    "Set the number of parallel jobs when compiling files",
                    {"jobs", 'j'},
                    0) {}
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

    args::MapFlag<std::string, dds::log::level> log_level{
        parser,
        "log-level",
        "Set the logging level",
        {"log-level", 'l'},
        {
            {"trace", dds::log::level::trace},
            {"debug", dds::log::level::debug},
            {"info", dds::log::level::info},
            {"warn", dds::log::level::warn},
            {"error", dds::log::level::error},
            {"critical", dds::log::level::critical},
        },
        dds::log::level::info,
    };

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

        args::Flag import_stdin{cmd, "stdin", "Import JSON from stdin", {"stdin"}};
        args::Flag init{cmd, "initial", "Re-import the initial catalog contents", {"initial"}};
        args::ValueFlagList<std::string>
            json_paths{cmd,
                       "json",
                       "Import catalog entries from the given JSON files",
                       {"json", 'j'}};

        int run() {
            auto cat = cat_path.open();
            if (init.Get()) {
                cat.import_initial();
            }
            for (const auto& json_fpath : json_paths.Get()) {
                cat.import_json_file(json_fpath);
            }
            if (import_stdin.Get()) {
                std::ostringstream strm;
                strm << std::cin.rdbuf();
                cat.import_json_str(strm.str());
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
                auto            id = dds::package_id::parse(req);
                dds::dym_target dym;
                auto            info = cat.get(id);
                if (!info) {
                    dds::throw_user_error<dds::errc::no_such_catalog_package>(
                        "No package in the catalog matched the ID '{}'.{}",
                        req,
                        dym.sentence_suffix());
                }
                auto tsd      = dds::get_package_sdist(*info);
                auto out_path = out.Get();
                auto dest     = out_path / id.to_string();
                dds_log(info, "Create sdist at {}", dest.string());
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

        string_flag description{cmd, "description", "A description of the package", {"desc"}};

        int run() {
            auto ident = dds::package_id::parse(pkg_id.Get());

            std::vector<dds::dependency> deps;
            for (const auto& dep : this->deps.Get()) {
                auto dep_id = dds::package_id::parse(dep);
                assert(false && "TODO");
                // deps.push_back({dep_id.name, dep_id.version});
            }

            dds::package_info info{ident, std::move(deps), description.Get(), {}};

            if (git_url) {
                if (!git_ref) {
                    dds::throw_user_error<dds::errc::git_url_ref_mutual_req>();
                }
                auto git = dds::git_remote_listing{git_url.Get(), git_ref.Get(), std::nullopt, {}};
                if (auto_lib) {
                    git.auto_lib = lm::split_usage_string(auto_lib.Get());
                }
                info.remote = std::move(git);
            } else if (git_ref) {
                dds::throw_user_error<dds::errc::git_url_ref_mutual_req>();
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

    struct {
        cli_catalog&  parent;
        args::Command cmd{parent.cat_group,
                          "show",
                          "Show information about a single package in the catalog"};

        catalog_path_flag             cat_path{cmd};
        args::Positional<std::string> ident{cmd,
                                            "package-id",
                                            "A package identifier to show",
                                            args::Options::Required};

        void print_remote_info(const dds::git_remote_listing& git) {
            std::cout << "Git URL:  " << git.url << '\n';
            std::cout << "Git Ref:  " << git.ref << '\n';
            if (git.auto_lib) {
                std::cout << "Auto-lib: " << git.auto_lib->name << "/" << git.auto_lib->namespace_
                          << '\n';
            }
        }

        void print_remote_info(std::monostate) {
            std::cout << "THIS ENTRY IS MISSING REMOTE INFORMATION!\n";
        }

        int run() {
            auto pk_id = dds::package_id::parse(ident.Get());
            auto cat   = cat_path.open();
            auto pkg   = cat.get(pk_id);
            if (!pkg) {
                dds_log(error, "No package '{}' in the catalog", pk_id.to_string());
                return 1;
            }
            std::cout << "Name:     " << pkg->ident.name << '\n'
                      << "Version:  " << pkg->ident.version << '\n';

            for (const auto& dep : pkg->deps) {
                std::cout << "Depends:  " << dep.to_string() << '\n';
            }

            std::visit([&](const auto& remote) { print_remote_info(remote); }, pkg->remote);
            std::cout << "Description:\n    " << pkg->description << '\n';

            return 0;
        }
    } show{*this};

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
        } else if (show.cmd) {
            return show.run();
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
                    dds_log(info, "{}:", name);
                    for (const dds::sdist& sd : grp) {
                        dds_log(info, "  - {}", sd.manifest.pkg_id.version.to_string());
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
        args::Command cmd{parent.repo_group,
                          "import",
                          "Import a source distribution archive file into the repository"};
        common_flags  _common{cmd};

        args::PositionalList<dds::fs::path>
            sdist_paths{cmd, "sdist-path", "Path to one or more source distribution archive"};

        args::Flag force{cmd,
                         "replace-if-exists",
                         "Replace an existing package in the repository",
                         {"replace"}};

        args::Flag import_stdin{cmd,
                                "import-stdin",
                                "Import a source distribution tarball from stdin",
                                {"stdin"}};

        int run() {
            auto import_sdists = [&](dds::repository repo) {
                auto if_exists_action
                    = force.Get() ? dds::if_exists::replace : dds::if_exists::throw_exc;
                for (auto& tgz_path : sdist_paths.Get()) {
                    auto tmp_sd = dds::expand_sdist_targz(tgz_path);
                    repo.add_sdist(tmp_sd.sdist, if_exists_action);
                }
                if (import_stdin) {
                    auto tmp_sd = dds::expand_sdist_from_istream(std::cin, "<stdin>");
                    repo.add_sdist(tmp_sd.sdist, if_exists_action);
                }
                return 0;
            };
            return dds::repository::with_repository(parent.where.Get(),
                                                    dds::repo_flags::write_lock
                                                        | dds::repo_flags::create_if_absent,
                                                    import_sdists);
        }
    } import_{*this};

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
        } else if (import_.cmd) {
            return import_.run();
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

        path_flag out{cmd, "out", "The destination of the source distribution", {"out"}};

        args::Flag force{cmd,
                         "replace-if-exists",
                         "Forcibly replace an existing distribution",
                         {"replace"}};

        int run() {
            dds::sdist_params params;
            params.project_dir   = project.root.Get();
            params.dest_path     = out.Get();
            params.force         = force.Get();
            params.include_apps  = true;
            params.include_tests = true;
            auto pkg_man         = dds::package_manifest::load_from_directory(project.root.Get());
            if (!pkg_man) {
                dds::throw_user_error<dds::errc::invalid_pkg_manifest>(
                    "Creating a source distribution requires a package manifest");
            }
            std::string default_filename = fmt::format("{}@{}.tar.gz",
                                                       pkg_man->pkg_id.name,
                                                       pkg_man->pkg_id.version.to_string());
            auto        default_filepath = dds::fs::current_path() / default_filename;
            auto        out_path         = out.Matched() ? out.Get() : default_filepath;
            dds::create_sdist_targz(out_path, params);
            dds_log(info, "Generate source distribution at [{}]", out_path.string());
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

void load_project_deps(dds::builder&                bd,
                       const dds::package_manifest& man,
                       dds::path_ref                cat_path,
                       dds::path_ref                repo_path) {
    auto cat = dds::catalog::open(cat_path);
    // Build the dependencies
    dds::repository::with_repository(  //
        repo_path,
        dds::repo_flags::write_lock | dds::repo_flags::create_if_absent,
        [&](dds::repository repo) {
            // Download dependencies
            auto deps = repo.solve(man.dependencies, cat);
            dds::get_all(deps, repo, cat);
            for (const dds::package_id& pk : deps) {
                auto sdist_ptr = repo.find(pk);
                assert(sdist_ptr);
                dds::sdist_build_params deps_params;
                deps_params.subdir
                    = dds::fs::path("_deps") / sdist_ptr->manifest.pkg_id.to_string();
                bd.add(*sdist_ptr, deps_params);
            }
        });
}

dds::builder create_project_builder(dds::path_ref                  pr_dir,
                                    dds::path_ref                  cat_path,
                                    dds::path_ref                  repo_path,
                                    bool                           load_deps,
                                    const dds::sdist_build_params& project_params) {
    auto man = dds::package_manifest::load_from_directory(pr_dir).value_or(dds::package_manifest{});

    dds::builder builder;
    if (load_deps) {
        load_project_deps(builder, man, cat_path, repo_path);
    }
    builder.add(dds::sdist{std::move(man), pr_dir}, project_params);
    return builder;
}

/*
 ######   #######  ##     ## ########  #### ##       ########
##    ## ##     ## ###   ### ##     ##  ##  ##       ##
##       ##     ## #### #### ##     ##  ##  ##       ##
##       ##     ## ## ### ## ########   ##  ##       ######
##       ##     ## ##     ## ##         ##  ##       ##
##    ## ##     ## ##     ## ##         ##  ##       ##
 ######   #######  ##     ## ##        #### ######## ########
*/

struct cli_compile_file {
    cli_base&     base;
    args::Command cmd{base.cmd_group, "compile-file", "Compile a single file"};

    common_flags _flags{cmd};

    common_project_flags project{cmd};

    catalog_path_flag cat_path{cmd};
    repo_path_flag    repo_path{cmd};

    args::Flag     no_warnings{cmd, "no-warnings", "Disable compiler warnings", {"no-warnings"}};
    toolchain_flag tc_filepath{cmd};

    path_flag
        lm_index{cmd,
                 "lm_index",
                 "Path to an existing libman index from which to load deps (usually INDEX.lmi)",
                 {"lm-index", 'I'}};

    num_jobs_flag n_jobs{cmd};

    path_flag out{cmd,
                  "out",
                  "The root build directory",
                  {"out"},
                  dds::fs::current_path() / "_build"};

    args::PositionalList<dds::fs::path> source_files{cmd,
                                                     "source-files",
                                                     "One or more source files to compile"};

    int run() {
        dds::sdist_build_params main_params = {
            .subdir          = "",
            .build_tests     = true,
            .build_apps      = true,
            .enable_warnings = !no_warnings.Get(),
        };
        auto bd = create_project_builder(project.root.Get(),
                                         cat_path.Get(),
                                         repo_path.Get(),
                                         /* load_deps = */ !lm_index,
                                         main_params);

        bd.compile_files(source_files.Get(),
                         {
                             .out_root = out.Get(),
                             .existing_lm_index
                             = lm_index ? std::make_optional(lm_index.Get()) : std::nullopt,
                             .emit_lmi      = {},
                             .toolchain     = tc_filepath.get_toolchain(),
                             .parallel_jobs = n_jobs.Get(),
                         });
        return 0;
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

    path_flag
        lm_index{cmd,
                 "lm_index",
                 "Path to an existing libman index from which to load deps (usually INDEX.lmi)",
                 {"lm-index", 'I'}};

    num_jobs_flag n_jobs{cmd};

    path_flag out{cmd,
                  "out",
                  "The root build directory",
                  {"out"},
                  dds::fs::current_path() / "_build"};

    int run() {
        dds::sdist_build_params main_params = {
            .subdir          = "",
            .build_tests     = !no_tests.Get(),
            .run_tests       = !no_tests.Get(),
            .build_apps      = !no_apps.Get(),
            .enable_warnings = !no_warnings.Get(),
        };
        auto bd = create_project_builder(project.root.Get(),
                                         cat_path.Get(),
                                         repo_path.Get(),
                                         /* load_deps = */ !lm_index,
                                         main_params);

        bd.build({
            .out_root          = out.Get(),
            .existing_lm_index = lm_index ? std::make_optional(lm_index.Get()) : std::nullopt,
            .emit_lmi          = {},
            .toolchain         = tc_filepath.get_toolchain(),
            .parallel_jobs     = n_jobs.Get(),
        });
        return 0;
    }
};

/*
########  ##     ## #### ##       ########          ########  ######## ########   ######
##     ## ##     ##  ##  ##       ##     ##         ##     ## ##       ##     ## ##    ##
##     ## ##     ##  ##  ##       ##     ##         ##     ## ##       ##     ## ##
########  ##     ##  ##  ##       ##     ## ####### ##     ## ######   ########   ######
##     ## ##     ##  ##  ##       ##     ##         ##     ## ##       ##              ##
##     ## ##     ##  ##  ##       ##     ##         ##     ## ##       ##        ##    ##
########   #######  #### ######## ########          ########  ######## ##         ######
*/

struct cli_build_deps {
    cli_base&     base;
    args::Command cmd{base.cmd_group,
                      "build-deps",
                      "Build a set of dependencies and emit a libman index"};

    toolchain_flag    tc{cmd};
    repo_path_flag    repo_path{cmd};
    catalog_path_flag cat_path{cmd};
    num_jobs_flag     n_jobs{cmd};

    args::ValueFlagList<dds::fs::path> deps_files{cmd,
                                                  "deps-file",
                                                  "Install dependencies from the named files",
                                                  {"deps", 'd'}};

    path_flag out_path{cmd,
                       "out-path",
                       "Directory where build results should be placed",
                       {"out", 'o'},
                       dds::fs::current_path() / "_deps"};

    path_flag lmi_path{cmd,
                       "lmi-path",
                       "Path to the output libman index file (INDEX.lmi)",
                       {"lmi-path"},
                       dds::fs::current_path() / "INDEX.lmi"};

    args::PositionalList<std::string> deps{cmd, "deps", "List of dependencies to install"};

    int run() {
        dds::build_params params;
        params.out_root      = out_path.Get();
        params.toolchain     = tc.get_toolchain();
        params.parallel_jobs = n_jobs.Get();
        params.emit_lmi      = lmi_path.Get();

        dds::builder            bd;
        dds::sdist_build_params sdist_params;

        auto all_file_deps = deps_files.Get()  //
            | ranges::views::transform([&](auto dep_fpath) {
                                 dds_log(info, "Reading deps from {}", dep_fpath.string());
                                 return dds::dependency_manifest::from_file(dep_fpath).dependencies;
                             })
            | ranges::actions::join;

        auto cmd_deps = ranges::views::transform(deps.Get(), [&](auto dep_str) {
            return dds::dependency::parse_depends_string(dep_str);
        });

        auto all_deps = ranges::views::concat(all_file_deps, cmd_deps) | ranges::to_vector;

        auto cat = cat_path.open();
        dds::repository::with_repository(  //
            repo_path.Get(),
            dds::repo_flags::write_lock | dds::repo_flags::create_if_absent,
            [&](dds::repository repo) {
                // Download dependencies
                dds_log(info, "Loading {} dependencies", all_deps.size());
                auto deps = repo.solve(all_deps, cat);
                dds::get_all(deps, repo, cat);
                for (const dds::package_id& pk : deps) {
                    auto sdist_ptr = repo.find(pk);
                    assert(sdist_ptr);
                    dds::sdist_build_params deps_params;
                    deps_params.subdir = sdist_ptr->manifest.pkg_id.to_string();
                    dds_log(info, "Dependency: {}", sdist_ptr->manifest.pkg_id.to_string());
                    bd.add(*sdist_ptr, deps_params);
                }
            });

        bd.build(params);
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

int main_fn(const std::vector<std::string>& argv) {
    spdlog::set_pattern("[%H:%M:%S] [%^%-5l%$] %v");
    args::ArgumentParser parser("DDS - The drop-dead-simple library manager");

    cli_base         cli{parser};
    cli_compile_file compile_file{cli};
    cli_build        build{cli};
    cli_sdist        sdist{cli};
    cli_repo         repo{cli};
    cli_catalog      catalog{cli};
    cli_build_deps   build_deps{cli};

    try {
        parser.ParseCLI(argv);
    } catch (const args::Help&) {
        std::cout << parser;
        return 0;
    } catch (const args::Error& e) {
        std::cerr << parser;
        std::cerr << e.what() << '\n';
        return 1;
    }

    dds::install_signal_handlers();
    dds::log::current_log_level = cli.log_level.Get();

    try {
        if (cli._verify_ident) {
            std::cout << "yes\n";
            return 0;
        } else if (compile_file.cmd) {
            return compile_file.run();
        } else if (build.cmd) {
            return build.run();
        } else if (sdist.cmd) {
            return sdist.run();
        } else if (repo.cmd) {
            return repo.run();
        } else if (catalog.cmd) {
            return catalog.run();
        } else if (build_deps.cmd) {
            return build_deps.run();
        } else {
            assert(false);
            std::terminate();
        }
    } catch (const dds::user_cancelled&) {
        dds_log(critical, "Operation cancelled by user");
        return 2;
    } catch (const dds::error_base& e) {
        dds_log(error, "{}", e.what());
        dds_log(error, "{}", e.explanation());
        dds_log(error, "Refer: {}", e.error_reference());
        return 1;
    } catch (const std::exception& e) {
        dds_log(critical, e.what());
        return 2;
    }
}

#if NEO_OS_IS_WINDOWS
std::string wstr_to_u8str(std::wstring_view in) {
    if (in.empty()) {
        return "";
    }
    auto req_size = ::WideCharToMultiByte(CP_UTF8,
                                          0,
                                          in.data(),
                                          static_cast<int>(in.size()),
                                          nullptr,
                                          0,
                                          nullptr,
                                          nullptr);
    neo_assert(invariant,
               req_size > 0,
               "Failed to convert given unicode string for main() argv",
               req_size,
               std::system_category().message(::GetLastError()),
               ::GetLastError());
    std::string ret;
    ret.resize(req_size);
    ::WideCharToMultiByte(CP_UTF8,
                          0,
                          in.data(),
                          static_cast<int>(in.size()),
                          ret.data(),
                          static_cast<int>(ret.size()),
                          nullptr,
                          nullptr);
    return ret;
}

int wmain(int argc, wchar_t** argv) {
    std::vector<std::string> u8_argv;
    ::setlocale(LC_ALL, ".utf8");
    for (int i = 1; i < argc; ++i) {
        u8_argv.emplace_back(wstr_to_u8str(argv[i]));
    }
    return main_fn(u8_argv);
}
#else
int main(int argc, char** argv) { return main_fn({argv + 1, argv + argc}); }
#endif
