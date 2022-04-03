#include <bpt/toolchain/from_json.hpp>

#include <bpt/util/proc.hpp>

#include <catch2/catch.hpp>

namespace {

struct test {
    std::string_view given;
    std::string_view compile;
    std::string_view with_warnings;
    std::string_view ar;
    std::string_view link;
};

void check_tc_compile(struct test test) {
    auto tc = bpt::parse_toolchain_json5(test.given);

    bpt::compile_file_spec cf;
    cf.source_path = "foo.cpp";
    cf.out_path    = "foo.o";
    auto cf_cmd    = tc.create_compile_command(cf, bpt::fs::current_path(), bpt::toolchain_knobs{});
    auto cf_cmd_str = bpt::quote_command(cf_cmd.command);
    CHECK(cf_cmd_str == test.compile);

    cf.enable_warnings = true;
    cf_cmd     = tc.create_compile_command(cf, bpt::fs::current_path(), bpt::toolchain_knobs{});
    cf_cmd_str = bpt::quote_command(cf_cmd.command);
    CHECK(cf_cmd_str == test.with_warnings);

    bpt::archive_spec ar_spec;
    ar_spec.input_files.push_back("foo.o");
    ar_spec.input_files.push_back("bar.o");
    ar_spec.out_path = "stuff.a";
    auto ar_cmd
        = tc.create_archive_command(ar_spec, bpt::fs::current_path(), bpt::toolchain_knobs{});
    auto ar_cmd_str = bpt::quote_command(ar_cmd);
    CHECK(ar_cmd_str == test.ar);

    bpt::link_exe_spec exe_spec;
    exe_spec.inputs.push_back("foo.o");
    exe_spec.inputs.push_back("bar.a");
    exe_spec.output  = "meow.exe";
    auto exe_cmd     = tc.create_link_executable_command(exe_spec,
                                                     bpt::fs::current_path(),
                                                     bpt::toolchain_knobs{});
    auto exe_cmd_str = bpt::quote_command(exe_cmd);
    CHECK(exe_cmd_str == test.link);
}

}  // namespace

TEST_CASE("Generating toolchain commands") {
    check_tc_compile(test{
        .given         = "{compiler_id: 'gnu'}",
        .compile       = "g++ -MD -MF foo.o.d -MQ foo.o -c foo.cpp -ofoo.o -fPIC -pthread",
        .with_warnings = "g++ -Wall -Wextra -Wpedantic -Wconversion -MD -MF foo.o.d -MQ "
                         "foo.o -c foo.cpp -ofoo.o -fPIC -pthread",
        .ar   = "ar rcs stuff.a foo.o bar.o",
        .link = "g++ -fPIC foo.o bar.a -pthread -omeow.exe",
    });

    check_tc_compile(test{
        .given         = "{compiler_id: 'gnu', debug: true}",
        .compile       = "g++ -MD -MF foo.o.d -MQ foo.o -c foo.cpp -ofoo.o -g -fPIC -pthread",
        .with_warnings = "g++ -Wall -Wextra -Wpedantic -Wconversion -MD -MF foo.o.d -MQ "
                         "foo.o -c foo.cpp -ofoo.o -g -fPIC -pthread",
        .ar   = "ar rcs stuff.a foo.o bar.o",
        .link = "g++ -fPIC foo.o bar.a -pthread -omeow.exe -g",
    });

    check_tc_compile(test{
        .given         = "{compiler_id: 'gnu', debug: true, optimize: true}",
        .compile       = "g++ -MD -MF foo.o.d -MQ foo.o -c foo.cpp -ofoo.o -O2 -g -fPIC -pthread",
        .with_warnings = "g++ -Wall -Wextra -Wpedantic -Wconversion -MD -MF foo.o.d -MQ "
                         "foo.o -c foo.cpp -ofoo.o -O2 -g -fPIC -pthread",
        .ar   = "ar rcs stuff.a foo.o bar.o",
        .link = "g++ -fPIC foo.o bar.a -pthread -omeow.exe -O2 -g",
    });

    check_tc_compile(test{
        .given = "{compiler_id: 'gnu', debug: 'split', optimize: true}",
        .compile
        = "g++ -MD -MF foo.o.d -MQ foo.o -c foo.cpp -ofoo.o -O2 -g -gsplit-dwarf -fPIC -pthread",
        .with_warnings = "g++ -Wall -Wextra -Wpedantic -Wconversion -MD -MF foo.o.d -MQ "
                         "foo.o -c foo.cpp -ofoo.o -O2 -g -gsplit-dwarf -fPIC -pthread",
        .ar   = "ar rcs stuff.a foo.o bar.o",
        .link = "g++ -fPIC foo.o bar.a -pthread -omeow.exe -O2 -g -gsplit-dwarf",
    });

    check_tc_compile(test{
        .given   = "{compiler_id: 'gnu', cxx_version: 'gnu++20'}",
        .compile = "g++ -MD -MF foo.o.d -MQ foo.o -c foo.cpp -ofoo.o -std=gnu++20 -fPIC -pthread",
        .with_warnings = "g++ -Wall -Wextra -Wpedantic -Wconversion -MD -MF foo.o.d -MQ "
                         "foo.o -c foo.cpp -ofoo.o -std=gnu++20 -fPIC -pthread",
        .ar   = "ar rcs stuff.a foo.o bar.o",
        .link = "g++ -fPIC foo.o bar.a -pthread -omeow.exe",
    });

    check_tc_compile(test{
        .given = "{compiler_id: 'gnu', flags: '-fno-rtti', advanced: {cxx_compile_file: 'g++ "
                 "[flags] -c [in] -o[out]'}}",
        .compile = "g++ -MD -MF foo.o.d -MQ foo.o -c foo.cpp -ofoo.o -fno-rtti -fPIC -pthread",
        .with_warnings = "g++ -Wall -Wextra -Wpedantic -Wconversion -MD -MF foo.o.d -MQ "
                         "foo.o -c foo.cpp -ofoo.o -fno-rtti -fPIC -pthread",
        .ar   = "ar rcs stuff.a foo.o bar.o",
        .link = "g++ -fPIC foo.o bar.a -pthread -omeow.exe",
    });

    check_tc_compile(test{
        .given
        = "{compiler_id: 'gnu', flags: '-fno-rtti', advanced: {base_flags: '-fno-exceptions'}}",
        .compile = "g++ -MD -MF foo.o.d -MQ foo.o -c foo.cpp -ofoo.o -fno-rtti -fno-exceptions",
        .with_warnings = "g++ -Wall -Wextra -Wpedantic -Wconversion -MD -MF foo.o.d -MQ "
                         "foo.o -c foo.cpp -ofoo.o -fno-rtti -fno-exceptions",
        .ar   = "ar rcs stuff.a foo.o bar.o",
        .link = "g++ -fPIC foo.o bar.a -pthread -omeow.exe",
    });

    check_tc_compile(test{
        .given = "{compiler_id: 'gnu', flags: '-ansi', cxx_flags: '-fno-rtti', advanced: "
                 "{base_flags: '-fno-builtin', base_cxx_flags: '-fno-exceptions'}}",
        .compile = "g++ -MD -MF foo.o.d -MQ foo.o -c foo.cpp -ofoo.o -ansi -fno-rtti "
                   "-fno-builtin -fno-exceptions",
        .with_warnings
        = "g++ -Wall -Wextra -Wpedantic -Wconversion -MD -MF foo.o.d -MQ foo.o -c foo.cpp -ofoo.o "
          "-ansi -fno-rtti -fno-builtin -fno-exceptions",
        .ar   = "ar rcs stuff.a foo.o bar.o",
        .link = "g++ -fPIC foo.o bar.a -pthread -omeow.exe",
    });

    check_tc_compile(test{
        .given         = "{compiler_id: 'gnu', link_flags: '-mthumb'}",
        .compile       = "g++ -MD -MF foo.o.d -MQ foo.o -c foo.cpp -ofoo.o -fPIC -pthread",
        .with_warnings = "g++ -Wall -Wextra -Wpedantic -Wconversion -MD -MF foo.o.d -MQ "
                         "foo.o -c foo.cpp -ofoo.o -fPIC -pthread",
        .ar   = "ar rcs stuff.a foo.o bar.o",
        .link = "g++ -fPIC foo.o bar.a -pthread -omeow.exe -mthumb",
    });

    check_tc_compile(test{
        .given = "{compiler_id: 'gnu', link_flags: '-mthumb', advanced: {link_executable: 'g++ "
                 "[in] -o[out]'}}",
        .compile       = "g++ -MD -MF foo.o.d -MQ foo.o -c foo.cpp -ofoo.o -fPIC -pthread",
        .with_warnings = "g++ -Wall -Wextra -Wpedantic -Wconversion -MD -MF foo.o.d -MQ "
                         "foo.o -c foo.cpp -ofoo.o -fPIC -pthread",
        .ar   = "ar rcs stuff.a foo.o bar.o",
        .link = "g++ foo.o bar.a -omeow.exe -mthumb",
    });

    check_tc_compile(test{
        .given   = "{compiler_id: 'msvc'}",
        .compile = "cl.exe /showIncludes /c foo.cpp /Fofoo.o /MT /nologo /permissive- /EHsc",
        .with_warnings
        = "cl.exe /W4 /showIncludes /c foo.cpp /Fofoo.o /MT /nologo /permissive- /EHsc",
        .ar   = "lib /nologo /OUT:stuff.a foo.o bar.o",
        .link = "cl.exe /nologo /EHsc foo.o bar.a /Femeow.exe /MT",
    });

    check_tc_compile(test{
        .given   = "{compiler_id: 'msvc', debug: true}",
        .compile = "cl.exe /showIncludes /c foo.cpp /Fofoo.o /MTd /Z7 /nologo /permissive- /EHsc",
        .with_warnings
        = "cl.exe /W4 /showIncludes /c foo.cpp /Fofoo.o /MTd /Z7 /nologo /permissive- /EHsc",
        .ar   = "lib /nologo /OUT:stuff.a foo.o bar.o",
        .link = "cl.exe /nologo /EHsc foo.o bar.a /Femeow.exe /MTd /Z7",
    });

    check_tc_compile(test{
        .given   = "{compiler_id: 'msvc', debug: 'embedded'}",
        .compile = "cl.exe /showIncludes /c foo.cpp /Fofoo.o /MTd /Z7 /nologo /permissive- /EHsc",
        .with_warnings
        = "cl.exe /W4 /showIncludes /c foo.cpp /Fofoo.o /MTd /Z7 /nologo /permissive- /EHsc",
        .ar   = "lib /nologo /OUT:stuff.a foo.o bar.o",
        .link = "cl.exe /nologo /EHsc foo.o bar.a /Femeow.exe /MTd /Z7",
    });

    check_tc_compile(test{
        .given = "{compiler_id: 'msvc', debug: 'split'}",
        .compile
        = "cl.exe /showIncludes /c foo.cpp /Fofoo.o /MTd /Zi /FS /nologo /permissive- /EHsc",
        .with_warnings
        = "cl.exe /W4 /showIncludes /c foo.cpp /Fofoo.o /MTd /Zi /FS /nologo /permissive- /EHsc",
        .ar   = "lib /nologo /OUT:stuff.a foo.o bar.o",
        .link = "cl.exe /nologo /EHsc foo.o bar.a /Femeow.exe /MTd /Zi /FS",
    });

    check_tc_compile(test{
        .given   = "{compiler_id: 'msvc', flags: '-DFOO'}",
        .compile = "cl.exe /showIncludes /c foo.cpp /Fofoo.o /MT -DFOO /nologo /permissive- /EHsc",
        .with_warnings
        = "cl.exe /W4 /showIncludes /c foo.cpp /Fofoo.o /MT -DFOO /nologo /permissive- /EHsc",
        .ar   = "lib /nologo /OUT:stuff.a foo.o bar.o",
        .link = "cl.exe /nologo /EHsc foo.o bar.a /Femeow.exe /MT",
    });

    check_tc_compile(test{
        .given   = "{compiler_id: 'msvc', runtime: {static: false}}",
        .compile = "cl.exe /showIncludes /c foo.cpp /Fofoo.o /MD /nologo /permissive- /EHsc",
        .with_warnings
        = "cl.exe /W4 /showIncludes /c foo.cpp /Fofoo.o /MD /nologo /permissive- /EHsc",
        .ar   = "lib /nologo /OUT:stuff.a foo.o bar.o",
        .link = "cl.exe /nologo /EHsc foo.o bar.a /Femeow.exe /MD",
    });

    check_tc_compile(test{
        .given   = "{compiler_id: 'msvc', runtime: {static: false}, debug: true}",
        .compile = "cl.exe /showIncludes /c foo.cpp /Fofoo.o /MDd /Z7 /nologo /permissive- /EHsc",
        .with_warnings
        = "cl.exe /W4 /showIncludes /c foo.cpp /Fofoo.o /MDd /Z7 /nologo /permissive- /EHsc",
        .ar   = "lib /nologo /OUT:stuff.a foo.o bar.o",
        .link = "cl.exe /nologo /EHsc foo.o bar.a /Femeow.exe /MDd /Z7",
    });

    check_tc_compile(test{
        .given   = "{compiler_id: 'msvc', runtime: {static: false, debug: true}}",
        .compile = "cl.exe /showIncludes /c foo.cpp /Fofoo.o /MDd /nologo /permissive- /EHsc",
        .with_warnings
        = "cl.exe /W4 /showIncludes /c foo.cpp /Fofoo.o /MDd /nologo /permissive- /EHsc",
        .ar   = "lib /nologo /OUT:stuff.a foo.o bar.o",
        .link = "cl.exe /nologo /EHsc foo.o bar.a /Femeow.exe /MDd",
    });

    check_tc_compile(test{
        .given         = "{compiler_id: 'msvc', advanced: {base_cxx_flags: ''}}",
        .compile       = "cl.exe /showIncludes /c foo.cpp /Fofoo.o /MT /nologo /permissive-",
        .with_warnings = "cl.exe /W4 /showIncludes /c foo.cpp /Fofoo.o /MT /nologo /permissive-",
        .ar            = "lib /nologo /OUT:stuff.a foo.o bar.o",
        .link          = "cl.exe /nologo /EHsc foo.o bar.a /Femeow.exe /MT",
    });

    check_tc_compile(test{
        .given = "{compiler_id: 'msvc', cxx_version: 'c++latest'}",
        .compile
        = "cl.exe /showIncludes /c foo.cpp /Fofoo.o /MT /std:c++latest /nologo /permissive- /EHsc",
        .with_warnings = "cl.exe /W4 /showIncludes /c foo.cpp /Fofoo.o /MT /std:c++latest "
                         "/nologo /permissive- /EHsc",
        .ar   = "lib /nologo /OUT:stuff.a foo.o bar.o",
        .link = "cl.exe /nologo /EHsc foo.o bar.a /Femeow.exe /MT",
    });

    check_tc_compile(test{
        .given = "{compiler_id: 'msvc', advanced: {lang_version_flag_template: '/eggs:[version]'}, "
                 "cxx_version: 'meow'}",
        .compile
        = "cl.exe /showIncludes /c foo.cpp /Fofoo.o /MT /eggs:meow /nologo /permissive- /EHsc",
        .with_warnings
        = "cl.exe /W4 /showIncludes /c foo.cpp /Fofoo.o /MT /eggs:meow /nologo /permissive- /EHsc",
        .ar   = "lib /nologo /OUT:stuff.a foo.o bar.o",
        .link = "cl.exe /nologo /EHsc foo.o bar.a /Femeow.exe /MT",
    });
}

TEST_CASE("Manipulate a toolchain and file compilation") {
    auto tc = bpt::parse_toolchain_json5("{compiler_id: 'gnu'}");

    bpt::compile_file_spec cfs;
    cfs.source_path = "foo.cpp";
    cfs.out_path    = "foo.o";
    auto cmd = tc.create_compile_command(cfs, bpt::fs::current_path(), bpt::toolchain_knobs{});
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
                                    bpt::fs::current_path(),
                                    bpt::toolchain_knobs{.is_tty = true});
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
    cmd = tc.create_compile_command(cfs, bpt::fs::current_path(), bpt::toolchain_knobs{});
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
