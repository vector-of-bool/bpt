#include <dds/build.hpp>
#include <dds/lm_parse.hpp>
#include <dds/util.hpp>
#include <dds/logging.hpp>

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

struct cli_build {
    cli_base&     base;
    args::Command cmd{base.cmd_group, "build", "Build a library"};

    args::HelpFlag _help{cmd, "help", "Display this help message and exit", {'h', "help"}};

    path_flag lib_dir{cmd,
                      "lib_dir",
                      "The path to the directory containing the library",
                      {"lib-dir"},
                      dds::fs::current_path()};
    path_flag out_dir{cmd,
                      "out_dir",
                      "The directory in which to write the built files",
                      {"out-dir"},
                      dds::fs::current_path() / "_build"};

    string_flag export_name{cmd,
                            "export_name",
                            "Set the name of the export",
                            {"export-name", 'n'},
                            dds::fs::current_path().filename()};

    path_flag tc_filepath{cmd,
                          "toolchain_file",
                          "Path to the toolchain file to use",
                          {"toolchain-file", 'T'},
                          dds::fs::current_path() / "toolchain.dds"};

    args::Flag build_tests{cmd, "build_tests", "Build the tests", {"tests"}};
    args::Flag export_{cmd, "export_dir", "Generate a library export", {"export", 'E'}};

    args::Flag enable_warnings{cmd,
                               "enable_warnings",
                               "Enable compiler warnings",
                               {"--warnings", 'W'}};

    int run() {
        dds::build_params params;
        params.root           = lib_dir.Get();
        params.out_root       = out_dir.Get();
        params.toolchain_file = tc_filepath.Get();
        params.export_name    = export_name.Get();
        params.do_export      = export_.Get();
        params.build_tests    = build_tests.Get();
        params.enable_warnings = enable_warnings.Get();
        dds::library_manifest man;
        const auto man_filepath = params.root / "manifest.dds";
        if (exists(man_filepath)) {
            man = dds::library_manifest::load_from_file(man_filepath);
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

    try {
        if (build.cmd) {
            return build.run();
        } else {
            assert(false);
            std::terminate();
        }
    } catch (const std::exception& e) {
        spdlog::critical(e.what());
        return 2;
    }
}
