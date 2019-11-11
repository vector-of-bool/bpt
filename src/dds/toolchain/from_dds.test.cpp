#include <dds/toolchain/from_dds.hpp>

#include <dds/proc.hpp>

// #include <dds/util.test.hpp>
#include <catch2/catch.hpp>

namespace {

void check_tc_compile(std::string_view tc_content,
                      std::string_view expected_compile,
                      std::string_view expected_compile_warnings,
                      std::string_view expected_ar,
                      std::string_view expected_exe) {
    auto tc        = dds::parse_toolchain_dds(tc_content);

    dds::compile_file_spec cf;
    cf.source_path  = "foo.cpp";
    cf.out_path     = "foo.o";
    auto cf_cmd     = tc.create_compile_command(cf);
    auto cf_cmd_str = dds::quote_command(cf_cmd);
    CHECK(cf_cmd_str == expected_compile);

    cf.enable_warnings = true;
    cf_cmd             = tc.create_compile_command(cf);
    cf_cmd_str         = dds::quote_command(cf_cmd);
    CHECK(cf_cmd_str == expected_compile_warnings);

    dds::archive_spec ar_spec;
    ar_spec.input_files.push_back("foo.o");
    ar_spec.input_files.push_back("bar.o");
    ar_spec.out_path = "stuff.a";
    auto ar_cmd      = tc.create_archive_command(ar_spec);
    auto ar_cmd_str  = dds::quote_command(ar_cmd);
    CHECK(ar_cmd_str == expected_ar);

    dds::link_exe_spec exe_spec;
    exe_spec.inputs.push_back("foo.o");
    exe_spec.inputs.push_back("bar.a");
    exe_spec.output  = "meow.exe";
    auto exe_cmd     = tc.create_link_executable_command(exe_spec);
    auto exe_cmd_str = dds::quote_command(exe_cmd);
    CHECK(exe_cmd_str == expected_exe);
}

TEST_CASE("Generating toolchain commands") {
    check_tc_compile("Compiler-ID: GNU",
                     "g++ -fPIC -fdiagnostics-color -pthread -c foo.cpp -ofoo.o",
                     "g++ -fPIC -fdiagnostics-color -pthread -Wall -Wextra -Wpedantic -Wconversion "
                     "-c foo.cpp -ofoo.o",
                     "ar rcs stuff.a foo.o bar.o",
                     "g++ -fPIC -fdiagnostics-color foo.o bar.a -pthread -lstdc++fs -omeow.exe");

    check_tc_compile(
        "Compiler-ID: GNU\nDebug: True",
        "g++ -g -fPIC -fdiagnostics-color -pthread -c foo.cpp -ofoo.o",
        "g++ -g -fPIC -fdiagnostics-color -pthread -Wall -Wextra -Wpedantic -Wconversion "
        "-c foo.cpp -ofoo.o",
        "ar rcs stuff.a foo.o bar.o",
        "g++ -fPIC -fdiagnostics-color foo.o bar.a -pthread -lstdc++fs -omeow.exe -g");

    check_tc_compile(
        "Compiler-ID: GNU\nDebug: True\nOptimize: True",
        "g++ -O2 -g -fPIC -fdiagnostics-color -pthread -c foo.cpp -ofoo.o",
        "g++ -O2 -g -fPIC -fdiagnostics-color -pthread -Wall -Wextra -Wpedantic -Wconversion "
        "-c foo.cpp -ofoo.o",
        "ar rcs stuff.a foo.o bar.o",
        "g++ -fPIC -fdiagnostics-color foo.o bar.a -pthread -lstdc++fs -omeow.exe -O2 -g");

    check_tc_compile("Compiler-ID: MSVC",
                     "cl.exe /MT /nologo /permissive- /c foo.cpp /Fofoo.o /EHsc",
                     "cl.exe /MT /nologo /W4 /permissive- /c foo.cpp /Fofoo.o /EHsc",
                     "lib /nologo /OUT:stuff.a foo.o bar.o",
                     "cl.exe /nologo /EHsc foo.o bar.a /Femeow.exe /MT");

    check_tc_compile("Compiler-ID: MSVC\nDebug: True",
                     "cl.exe /Z7 /DEBUG /MTd /nologo /permissive- /c foo.cpp /Fofoo.o /EHsc",
                     "cl.exe /Z7 /DEBUG /MTd /nologo /W4 /permissive- /c foo.cpp /Fofoo.o /EHsc",
                     "lib /nologo /OUT:stuff.a foo.o bar.o",
                     "cl.exe /nologo /EHsc foo.o bar.a /Femeow.exe /Z7 /DEBUG /MTd");

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
