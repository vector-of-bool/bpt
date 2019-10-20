#include <dds/build.hpp>
#include <dds/logging.hpp>
#include <dds/repo/repo.hpp>
#include <dds/sdist.hpp>
#include <dds/util/fs.hpp>
#include <dds/util/paths.hpp>
#include <dds/util/signal.hpp>

#include <libman/parse.hpp>

#include <args.hxx>

#include <filesystem>
#include <iostream>

namespace {

using string_flag = args::ValueFlag<std::string>;
using path_flag   = args::ValueFlag<dds::fs::path>;

struct cli_base {
    args::ArgumentParser& parser;
    args::HelpFlag _help{parser, "help", "Display this help message and exit", {'h', "help"}};

    args::Group cmd_group{parser, "Available Commands"};
};

struct common_flags {
    args::Command& cmd;

    args::HelpFlag _help{cmd, "help", "Print this help message and exit", {'h', "help"}};
};

struct common_project_flags {
    args::Command& cmd;

    path_flag root{cmd,
                   "project_dir",
                   "Path to the directory containing the project",
                   {"project-dir"},
                   dds::fs::current_path()};
};

struct cli_repo {
    cli_base&     base;
    args::Command cmd{base.cmd_group, "repo", "Manage the package repository"};
    common_flags  _common{cmd};

    path_flag where{cmd, "dir", "Directory in which to initialize the repository", {'d', "dir"}};

    args::Group repo_group{cmd, "Repo subcommands"};

    struct {
        cli_repo&     parent;
        args::Command cmd{parent.repo_group, "ls", "List repository contents"};
        common_flags  _common{cmd};

        int run() {
            return dds::repository::with_repository(dds::repository::default_local_path(),
                                                    dds::repo_flags::none,
                                                    [&](auto) { return 0; });
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

struct cli_sdist {
    cli_base&     base;
    args::Command cmd{base.cmd_group, "sdist", "Create a source distribution of a project"};

    common_flags _common{cmd};

    common_project_flags project{cmd};

    path_flag out{cmd,
                  "out",
                  "The full destination of the source distribution",
                  {"out"},
                  dds::fs::current_path() / "project.dsd"};

    args::Flag force{cmd, "force", "Forcibly replace an existing result", {"force"}};
    args::Flag export_{cmd,
                       "export",
                       "Export the result into the local repository",
                       {'E', "export"}};

    int run() {
        dds::sdist_params params;
        params.project_dir = project.root.Get();
        params.dest_path   = out.Get();
        params.force       = force.Get();
        auto sdist         = dds::create_sdist(params);
        if (export_.Get()) {
            dds::repository::with_repository(  //
                dds::repository::default_local_path(),
                dds::repo_flags::create_if_absent | dds::repo_flags::write_lock,
                [&](dds::repository repo) {  //
                    repo.add_sdist(sdist);
                });
        }
        return 0;
    }
};

struct cli_build {
    cli_base&     base;
    args::Command cmd{base.cmd_group, "build", "Build a project"};

    common_flags _common{cmd};

    common_project_flags project{cmd};

    string_flag tc_filepath{cmd,
                            "toolchain_file",
                            "Path to the toolchain file to use",
                            {"toolchain", 'T'},
                            (dds::fs::current_path() / "toolchain.dds").string()};

    args::Flag build_tests{cmd, "build_tests", "Build the tests", {"tests", 't'}};
    args::Flag build_apps{cmd, "build_apps", "Build applications", {"apps", 'A'}};
    args::Flag export_{cmd, "export", "Generate a library export", {"export", 'E'}};

    path_flag lm_index{cmd,
                       "lm_index",
                       "Path to a libman index (usually INDEX.lmi)",
                       {"--lm-index", 'I'},
                       dds::fs::path()};

    args::Flag enable_warnings{cmd,
                               "enable_warnings",
                               "Enable compiler warnings",
                               {"warnings", 'W'}};

    args::Flag full{cmd,
                    "full",
                    "Build all optional components (tests, apps, warnings, export)",
                    {"full", 'F'}};

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

    dds::toolchain _get_toolchain() {
        const auto tc_path = tc_filepath.Get();
        if (tc_path.find(":") == 0) {
            auto default_tc = tc_path.substr(1);
            auto tc         = dds::toolchain::get_builtin(default_tc);
            if (!tc.has_value()) {
                throw std::runtime_error(
                    fmt::format("Invalid default toolchain name '{}'", default_tc));
            }
            return std::move(*tc);
        } else {
            return dds::toolchain::load_from_file(tc_path);
        }
    }

    int run() {
        dds::build_params params;
        params.root            = project.root.Get();
        params.out_root        = out.Get();
        params.toolchain       = _get_toolchain();
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
        if (full.Get()) {
            params.do_export       = true;
            params.build_tests     = true;
            params.build_apps      = true;
            params.enable_warnings = true;
        }
        dds::build(params, man);
        return 0;
    }
};

}  // namespace

int main(int argc, char** argv) {
    spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
    args::ArgumentParser parser("DDSLiM - The drop-dead-simple library manager");

    cli_base  cli{parser};
    cli_build build{cli};
    cli_sdist sdist{cli};
    cli_repo  repo{cli};
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
        if (build.cmd) {
            return build.run();
        } else if (sdist.cmd) {
            return sdist.run();
        } else if (repo.cmd) {
            return repo.run();
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
