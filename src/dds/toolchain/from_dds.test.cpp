#include <dds/toolchain/from_dds.hpp>

#include <dds/proc.hpp>

#include <dds/util.test.hpp>

namespace {

void check_tc_compile(std::string_view tc_content,
                      std::string_view compile,
                      std::string_view compile_warnings,
                      std::string_view ar,
                      std::string_view exe) {
    auto tc        = dds::parse_toolchain_dds(tc_content);
    bool any_error = false;

    dds::compile_file_spec cf;
    cf.source_path  = "foo.cpp";
    cf.out_path     = "foo.o";
    auto cf_cmd     = tc.create_compile_command(cf);
    auto cf_cmd_str = dds::quote_command(cf_cmd);
    if (cf_cmd_str != compile) {
        std::cerr << "Compile command came out incorrect!\n";
        std::cerr << "  Expected: " << compile << '\n';
        std::cerr << "  Actual:   " << cf_cmd_str << "\n\n";
        any_error = true;
    }

    cf.enable_warnings = true;
    cf_cmd             = tc.create_compile_command(cf);
    cf_cmd_str         = dds::quote_command(cf_cmd);
    if (cf_cmd_str != compile_warnings) {
        std::cerr << "Compile command (with warnings) came out incorrect!\n";
        std::cerr << "  Expected: " << compile_warnings << '\n';
        std::cerr << "  Actual:   " << cf_cmd_str << "\n\n";
        any_error = true;
    }

    dds::archive_spec ar_spec;
    ar_spec.input_files.push_back("foo.o");
    ar_spec.input_files.push_back("bar.o");
    ar_spec.out_path = "stuff.a";
    auto ar_cmd      = tc.create_archive_command(ar_spec);
    auto ar_cmd_str  = dds::quote_command(ar_cmd);
    if (ar_cmd_str != ar) {
        std::cerr << "Archive command came out incorrect!\n";
        std::cerr << "  Expected: " << ar << '\n';
        std::cerr << "  Actual:   " << ar_cmd_str << "\n\n";
        any_error = true;
    }

    dds::link_exe_spec exe_spec;
    exe_spec.inputs.push_back("foo.o");
    exe_spec.inputs.push_back("bar.a");
    exe_spec.output  = "meow.exe";
    auto exe_cmd     = tc.create_link_executable_command(exe_spec);
    auto exe_cmd_str = dds::quote_command(exe_cmd);
    if (exe_cmd_str != exe) {
        std::cerr << "Executable linking command came out incorrect!\n";
        std::cerr << "  Expected: " << exe << '\n';
        std::cerr << "  Actual:   " << exe_cmd_str << "\n\n";
        any_error = true;
    }

    if (any_error) {
        std::cerr << "The error-producing toolchain file content:\n" << tc_content << '\n';
        dds::S_failed_checks++;
    }
}

void run_tests() {
    check_tc_compile("Compiler-ID: GNU",
                     "g++ -fPIC -fdiagnostics-color -pthread -c foo.cpp -ofoo.o",
                     "g++ -fPIC -fdiagnostics-color -pthread -Wall -Wextra -Wpedantic -Wconversion "
                     "-c foo.cpp -ofoo.o",
                     "ar rcs stuff.a foo.o bar.o",
                     "g++ -fPIC -fdiagnostics-color foo.o bar.a -pthread -lstdc++fs -omeow.exe");

    auto tc = dds::parse_toolchain_dds(R"(
    Compiler-ID: GNU
)");

    dds::compile_file_spec cfs;
    cfs.source_path = "foo.cpp";
    cfs.out_path    = "foo.o";
    auto cmd        = tc.create_compile_command(cfs);
    CHECK(cmd
          == std::vector<std::string>{"g++",
                                      "-fPIC",
                                      "-fdiagnostics-color",
                                      "-pthread",
                                      "-c",
                                      "foo.cpp",
                                      "-ofoo.o"});

    cfs.definitions.push_back("FOO=BAR");
    cmd = tc.create_compile_command(cfs);
    CHECK(cmd
          == std::vector<std::string>{"g++",
                                      "-fPIC",
                                      "-fdiagnostics-color",
                                      "-pthread",
                                      "-D",
                                      "FOO=BAR",
                                      "-c",
                                      "foo.cpp",
                                      "-ofoo.o"});

    cfs.include_dirs.push_back("fake-dir");
    cmd = tc.create_compile_command(cfs);
    CHECK(cmd
          == std::vector<std::string>{"g++",
                                      "-fPIC",
                                      "-fdiagnostics-color",
                                      "-pthread",
                                      "-I",
                                      "fake-dir",
                                      "-D",
                                      "FOO=BAR",
                                      "-c",
                                      "foo.cpp",
                                      "-ofoo.o"});
}

}  // namespace

DDS_TEST_MAIN;