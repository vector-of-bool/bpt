#include "./import.hpp"

#include <dds/error/errors.hpp>

#include <catch2/catch.hpp>

TEST_CASE("An empty import is okay") {
    // An empty JSON with no packages in it
    auto pkgs = dds::parse_packages_json("{version: 1, packages: {}}");
    CHECK(pkgs.empty());
}

TEST_CASE("Valid/invalid package JSON5") {
    std::string_view bads[] = {
        // Invalid JSON:
        "",
        // Should be an object
        "[]",
        // Missing keys
        "{}",
        // Missing "packages"
        "{version: 1}",
        // Bad version
        "{version: 1.7, packages: {}}",
        "{version: [], packages: {}}",
        "{version: null, packages: {}}",
        // 'packages' should be an object
        "{version: 1, packages: []}",
        "{version: 1, packages: null}",
        "{version: 1, packages: 4}",
        "{version: 1, packages: 'lol'}",
        // Objects in 'packages' should be objects
        "{version:1, packages:{foo:null}}",
        "{version:1, packages:{foo:[]}}",
        "{version:1, packages:{foo:9}}",
        "{version:1, packages:{foo:'lol'}}",
        // Objects in 'packages' shuold have version strings
        "{version:1, packages:{foo:{'lol':{}}}}",
        "{version:1, packages:{foo:{'1.2':{}}}}",
        // No remote
        "{version:1, packages:{foo:{'1.2.3':{}}}}",
        // Bad empty git
        "{version:1, packages:{foo:{'1.2.3':{git:{}}}}}",
        // Git `url` and `ref` should be a string
        "{version:1, packages:{foo:{'1.2.3':{git:{url:2, ref:''}}}}}",
        "{version:1, packages:{foo:{'1.2.3':{git:{url:'', ref:2}}}}}",
        // 'auto-lib' should be a usage string
        "{version:1, packages:{foo:{'1.2.3':{git:{url:'', ref:'', 'auto-lib':3}}}}}",
        "{version:1, packages:{foo:{'1.2.3':{git:{url:'', ref:'', 'auto-lib':'ffasdf'}}}}}",
        // 'transform' should be an array
        R"(
            {
                version: 1,
                packages: {foo: {'1.2.3': {
                    git: {
                        url: '',
                        ref: '',
                        'auto-lib': 'a/b',
                        transform: 'lol hi',
                    }
                }}}
            }
        )",
    };

    for (auto bad : bads) {
        INFO("Bad: " << bad);
        CHECK_THROWS_AS(dds::parse_packages_json(bad),
                        dds::user_error<dds::errc::invalid_catalog_json>);
    }

    std::string_view goods[] = {
        // Basic empty:
        "{version:1, packages:{}}",
        // No versions for 'foo' is weird, but okay
        "{version:1, packages:{foo:{}}}",
        // Basic package with minimum info:
        "{version:1, packages:{foo:{'1.2.3':{git:{url:'', ref:''}}}}}",
        // Minimal auto-lib:
        "{version:1, packages:{foo:{'1.2.3':{git:{url:'', ref:'', 'auto-lib':'a/b'}}}}}",
        // Empty transforms:
        R"(
            {
                version: 1,
                packages: {foo: {'1.2.3': {
                    git: {
                        url: '',
                        ref: '',
                        'auto-lib': 'a/b',
                        transform: [],
                    }
                }}}
            }
        )",
        // Basic transform:
        R"(
            {
                version: 1,
                packages: {foo: {'1.2.3': {
                    git: {
                        url: '',
                        ref: '',
                        'auto-lib': 'a/b',
                        transform: [{
                            copy: {
                                from: 'here',
                                to: 'there',
                                include: [
                                    "*.c",
                                    "*.cpp",
                                    "*.h",
                                    '*.txt'
                                ]
                            }
                        }],
                    }
                }}}
            }
        )",
    };
    for (auto good : goods) {
        INFO("Parse: " << good);
        CHECK_NOTHROW(dds::parse_packages_json(good));
    }
}

TEST_CASE("Check a single object") {
    // An empty JSON with no packages in it
    auto pkgs = dds::parse_packages_json(R"({
        version: 1,
        packages: {
            foo: {
                '1.2.3': {
                    git: {
                        url: 'foo',
                        ref: 'fasdf',
                        'auto-lib': 'a/b',
                    }
                }
            }
        }
    })");
    REQUIRE(pkgs.size() == 1);
    CHECK(pkgs[0].ident.name == "foo");
    CHECK(pkgs[0].ident.to_string() == "foo@1.2.3");
    CHECK(std::holds_alternative<dds::git_remote_listing>(pkgs[0].remote));

    auto git = std::get<dds::git_remote_listing>(pkgs[0].remote);
    CHECK(git.url == "foo");
    CHECK(git.ref == "fasdf");
    REQUIRE(git.auto_lib);
    CHECK(git.auto_lib->namespace_ == "a");
    CHECK(git.auto_lib->name == "b");
}
