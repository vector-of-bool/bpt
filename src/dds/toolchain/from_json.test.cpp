#include <dds/toolchain/from_json.hpp>

#include <dds/proc.hpp>

#include <catch2/catch.hpp>

namespace {
void check_tc_compile(std::string_view tc_content,
                      std::string_view expected_compile,
                      std::string_view expected_compile_warnings,
                      std::string_view expected_ar,
                      std::string_view expected_exe) {
    auto tc = dds::parse_toolchain_json5(tc_content);

    dds::compile_file_spec cf;
    cf.source_path = "foo.cpp";
    cf.out_path    = "foo.o";
    auto cf_cmd    = tc.create_compile_command(cf, dds::fs::current_path(), dds::toolchain_knobs{});
    auto cf_cmd_str = dds::quote_command(cf_cmd.command);
    CHECK(cf_cmd_str == expected_compile);

    cf.enable_warnings = true;
    cf_cmd     = tc.create_compile_command(cf, dds::fs::current_path(), dds::toolchain_knobs{});
    cf_cmd_str = dds::quote_command(cf_cmd.command);
    CHECK(cf_cmd_str == expected_compile_warnings);

    dds::archive_spec ar_spec;
    ar_spec.input_files.push_back("foo.o");
    ar_spec.input_files.push_back("bar.o");
    ar_spec.out_path = "stuff.a";
    auto ar_cmd
        = tc.create_archive_command(ar_spec, dds::fs::current_path(), dds::toolchain_knobs{});
    auto ar_cmd_str = dds::quote_command(ar_cmd);
    CHECK(ar_cmd_str == expected_ar);

    dds::link_exe_spec exe_spec;
    exe_spec.inputs.push_back("foo.o");
    exe_spec.inputs.push_back("bar.a");
    exe_spec.output  = "meow.exe";
    auto exe_cmd     = tc.create_link_executable_command(exe_spec,
                                                     dds::fs::current_path(),
                                                     dds::toolchain_knobs{});
    auto exe_cmd_str = dds::quote_command(exe_cmd);
    CHECK(exe_cmd_str == expected_exe);
}

}  // namespace

TEST_CASE("Generating toolchain commands") {
    check_tc_compile("{compiler_id: 'gnu'}",
                     "g++ -MD -MF foo.o.d -MQ foo.o -c foo.cpp -ofoo.o -fPIC -pthread",
                     "g++ -Wall -Wextra -Wpedantic -Wconversion -MD -MF foo.o.d -MQ foo.o -c "
                     "foo.cpp -ofoo.o -fPIC -pthread",
                     "ar rcs stuff.a foo.o bar.o",
                     "g++ -fPIC foo.o bar.a -pthread -omeow.exe");

    check_tc_compile("{compiler_id: 'gnu', debug: true}",
                     "g++ -MD -MF foo.o.d -MQ foo.o -c foo.cpp -ofoo.o -g -fPIC -pthread",
                     "g++ -Wall -Wextra -Wpedantic -Wconversion -MD -MF foo.o.d -MQ foo.o -c "
                     "foo.cpp -ofoo.o -g -fPIC -pthread",
                     "ar rcs stuff.a foo.o bar.o",
                     "g++ -fPIC foo.o bar.a -pthread -omeow.exe -g");

    check_tc_compile("{compiler_id: 'gnu', debug: true, optimize: true}",
                     "g++ -MD -MF foo.o.d -MQ foo.o -c foo.cpp -ofoo.o -O2 -g -fPIC -pthread",
                     "g++ -Wall -Wextra -Wpedantic -Wconversion -MD -MF foo.o.d -MQ foo.o -c "
                     "foo.cpp -ofoo.o -O2 -g -fPIC -pthread",
                     "ar rcs stuff.a foo.o bar.o",
                     "g++ -fPIC foo.o bar.a -pthread -omeow.exe -O2 -g");

    check_tc_compile(
        "{compiler_id: 'gnu', debug: 'split', optimize: true}",
        "g++ -MD -MF foo.o.d -MQ foo.o -c foo.cpp -ofoo.o -O2 -g -gsplit-dwarf -fPIC -pthread",
        "g++ -Wall -Wextra -Wpedantic -Wconversion -MD -MF foo.o.d -MQ foo.o -c foo.cpp -ofoo.o "
        "-O2 -g -gsplit-dwarf -fPIC -pthread",
        "ar rcs stuff.a foo.o bar.o",
        "g++ -fPIC foo.o bar.a -pthread -omeow.exe -O2 -g -gsplit-dwarf");

    check_tc_compile(
        "{compiler_id: 'gnu', flags: '-fno-rtti', advanced: {cxx_compile_file: 'g++ [flags] -c "
        "[in] -o[out]'}}",
        "g++ -MD -MF foo.o.d -MQ foo.o -c foo.cpp -ofoo.o -fno-rtti -fPIC -pthread",
        "g++ -Wall -Wextra -Wpedantic -Wconversion -MD -MF foo.o.d -MQ foo.o -c foo.cpp -ofoo.o "
        "-fno-rtti -fPIC -pthread",
        "ar rcs stuff.a foo.o bar.o",
        "g++ -fPIC foo.o bar.a -pthread -omeow.exe");

    check_tc_compile(
        "{compiler_id: 'gnu', flags: '-fno-rtti', advanced: {base_compile_flags: "
        "'-fno-exceptions'}}",
        "g++ -MD -MF foo.o.d -MQ foo.o -c foo.cpp -ofoo.o -fno-rtti -fno-exceptions",
        "g++ -Wall -Wextra -Wpedantic -Wconversion -MD -MF foo.o.d -MQ foo.o -c foo.cpp -ofoo.o "
        "-fno-rtti -fno-exceptions",
        "ar rcs stuff.a foo.o bar.o",
        "g++ -fPIC foo.o bar.a -pthread -omeow.exe");

    check_tc_compile("{compiler_id: 'gnu', link_flags: '-mthumb'}",
                     "g++ -MD -MF foo.o.d -MQ foo.o -c foo.cpp -ofoo.o -fPIC -pthread",
                     "g++ -Wall -Wextra -Wpedantic -Wconversion -MD -MF foo.o.d -MQ foo.o -c "
                     "foo.cpp -ofoo.o -fPIC -pthread",
                     "ar rcs stuff.a foo.o bar.o",
                     "g++ -fPIC foo.o bar.a -pthread -omeow.exe -mthumb");

    check_tc_compile(
        "{compiler_id: 'gnu', link_flags: '-mthumb', advanced: {link_executable: 'g++ [in] "
        "-o[out]'}}",
        "g++ -MD -MF foo.o.d -MQ foo.o -c foo.cpp -ofoo.o -fPIC -pthread",
        "g++ -Wall -Wextra -Wpedantic -Wconversion -MD -MF foo.o.d -MQ foo.o -c foo.cpp -ofoo.o "
        "-fPIC -pthread",
        "ar rcs stuff.a foo.o bar.o",
        "g++ foo.o bar.a -omeow.exe -mthumb");

    check_tc_compile("{compiler_id: 'msvc'}",
                     "cl.exe /showIncludes /c foo.cpp /Fofoo.o /MT /EHsc /nologo /permissive-",
                     "cl.exe /W4 /showIncludes /c foo.cpp /Fofoo.o /MT /EHsc /nologo /permissive-",
                     "lib /nologo /OUT:stuff.a foo.o bar.o",
                     "cl.exe /nologo /EHsc foo.o bar.a /Femeow.exe /MT");

    check_tc_compile(
        "{compiler_id: 'msvc', debug: true}",
        "cl.exe /showIncludes /c foo.cpp /Fofoo.o /MTd /Z7 /EHsc /nologo /permissive-",
        "cl.exe /W4 /showIncludes /c foo.cpp /Fofoo.o /MTd /Z7 /EHsc /nologo /permissive-",
        "lib /nologo /OUT:stuff.a foo.o bar.o",
        "cl.exe /nologo /EHsc foo.o bar.a /Femeow.exe /MTd /Z7");

    check_tc_compile(
        "{compiler_id: 'msvc', debug: 'embedded'}",
        "cl.exe /showIncludes /c foo.cpp /Fofoo.o /MTd /Z7 /EHsc /nologo /permissive-",
        "cl.exe /W4 /showIncludes /c foo.cpp /Fofoo.o /MTd /Z7 /EHsc /nologo /permissive-",
        "lib /nologo /OUT:stuff.a foo.o bar.o",
        "cl.exe /nologo /EHsc foo.o bar.a /Femeow.exe /MTd /Z7");

    check_tc_compile(
        "{compiler_id: 'msvc', debug: 'split'}",
        "cl.exe /showIncludes /c foo.cpp /Fofoo.o /MTd /Zi /FS /EHsc /nologo /permissive-",
        "cl.exe /W4 /showIncludes /c foo.cpp /Fofoo.o /MTd /Zi /FS /EHsc /nologo /permissive-",
        "lib /nologo /OUT:stuff.a foo.o bar.o",
        "cl.exe /nologo /EHsc foo.o bar.a /Femeow.exe /MTd /Zi /FS");

    check_tc_compile(
        "{compiler_id: 'msvc', flags: '-DFOO'}",
        "cl.exe /showIncludes /c foo.cpp /Fofoo.o /MT -DFOO /EHsc /nologo /permissive-",
        "cl.exe /W4 /showIncludes /c foo.cpp /Fofoo.o /MT -DFOO /EHsc /nologo /permissive-",
        "lib /nologo /OUT:stuff.a foo.o bar.o",
        "cl.exe /nologo /EHsc foo.o bar.a /Femeow.exe /MT");

    check_tc_compile("{compiler_id: 'msvc', runtime: {static: false}}",
                     "cl.exe /showIncludes /c foo.cpp /Fofoo.o /MD /EHsc /nologo /permissive-",
                     "cl.exe /W4 /showIncludes /c foo.cpp /Fofoo.o /MD /EHsc /nologo /permissive-",
                     "lib /nologo /OUT:stuff.a foo.o bar.o",
                     "cl.exe /nologo /EHsc foo.o bar.a /Femeow.exe /MD");

    check_tc_compile(
        "{compiler_id: 'msvc', runtime: {static: false}, debug: true}",
        "cl.exe /showIncludes /c foo.cpp /Fofoo.o /MDd /Z7 /EHsc /nologo /permissive-",
        "cl.exe /W4 /showIncludes /c foo.cpp /Fofoo.o /MDd /Z7 /EHsc /nologo /permissive-",
        "lib /nologo /OUT:stuff.a foo.o bar.o",
        "cl.exe /nologo /EHsc foo.o bar.a /Femeow.exe /MDd /Z7");

    check_tc_compile("{compiler_id: 'msvc', runtime: {static: false, debug: true}}",
                     "cl.exe /showIncludes /c foo.cpp /Fofoo.o /MDd /EHsc /nologo /permissive-",
                     "cl.exe /W4 /showIncludes /c foo.cpp /Fofoo.o /MDd /EHsc /nologo /permissive-",
                     "lib /nologo /OUT:stuff.a foo.o bar.o",
                     "cl.exe /nologo /EHsc foo.o bar.a /Femeow.exe /MDd");
}

TEST_CASE("Manipulate a toolchain and file compilation") {
    auto tc = dds::parse_toolchain_json5("{compiler_id: 'gnu'}");

    dds::compile_file_spec cfs;
    cfs.source_path = "foo.cpp";
    cfs.out_path    = "foo.o";
    auto cmd = tc.create_compile_command(cfs, dds::fs::current_path(), dds::toolchain_knobs{});
    CHECK(cmd.command
          == std::vector<std::string>{"g++",
                                      "-MD",
                                      "-MF",
                                      "foo.o.d",
                                      "-MQ",
                                      "foo.o",
                                      "-c",
                                      "foo.cpp",
                                      "-ofoo.o",
                                      "-fPIC",
                                      "-pthread"});

    cfs.definitions.push_back("FOO=BAR");
    cmd = tc.create_compile_command(cfs,
                                    dds::fs::current_path(),
                                    dds::toolchain_knobs{.is_tty = true});
    CHECK(cmd.command
          == std::vector<std::string>{"g++",
                                      "-fdiagnostics-color",
                                      "-D",
                                      "FOO=BAR",
                                      "-MD",
                                      "-MF",
                                      "foo.o.d",
                                      "-MQ",
                                      "foo.o",
                                      "-c",
                                      "foo.cpp",
                                      "-ofoo.o",
                                      "-fPIC",
                                      "-pthread"});

    cfs.include_dirs.push_back("fake-dir");
    cmd = tc.create_compile_command(cfs, dds::fs::current_path(), dds::toolchain_knobs{});
    CHECK(cmd.command
          == std::vector<std::string>{"g++",
                                      "-I",
                                      "fake-dir",
                                      "-D",
                                      "FOO=BAR",
                                      "-MD",
                                      "-MF",
                                      "foo.o.d",
                                      "-MQ",
                                      "foo.o",
                                      "-c",
                                      "foo.cpp",
                                      "-ofoo.o",
                                      "-fPIC",
                                      "-pthread"});
}
