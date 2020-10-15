#include "./import.hpp"

#include <dds/error/errors.hpp>

#include <catch2/catch.hpp>

TEST_CASE("An empty import is okay") {
    // An empty JSON with no packages in it
    auto pkgs = dds::parse_packages_json("{version: 2, packages: {}}");
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
        "{version: 2}",
        // Bad version
        "{version: 2.7, packages: {}}",
        "{version: [], packages: {}}",
        "{version: null, packages: {}}",
        // 'packages' should be an object
        "{version: 2, packages: []}",
        "{version: 2, packages: null}",
        "{version: 2, packages: 4}",
        "{version: 2, packages: 'lol'}",
        // Objects in 'packages' should be objects
        "{version:2, packages:{foo:null}}",
        "{version:2, packages:{foo:[]}}",
        "{version:2, packages:{foo:9}}",
        "{version:2, packages:{foo:'lol'}}",
        // Objects in 'packages' shuold have version strings
        "{version:2, packages:{foo:{'lol':{}}}}",
        "{version:2, packages:{foo:{'1.2':{}}}}",
        // No remote
        "{version:2, packages:{foo:{'1.2.3':{}}}}",
        // Bad empty URL
        "{version:2, packages:{foo:{'1.2.3':{url: ''}}}}",
        // Git URL must have a fragment
        "{version:2, packages:{foo:{'1.2.3':{url:'git+http://example.com'}}}}",
        // 'auto-lib' should be a usage string
        "{version:2, packages:{foo:{'1.2.3':{url:'git+http://example.com?lm=lol#1.0}}}}",
        // 'transform' should be an array
        R"(
            {
                version: 2,
                packages: {foo: {'1.2.3': {
                    url: 'git+http://example.com#master,
                    transform: 'lol hi'
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
        "{version:2, packages:{}}",
        // No versions for 'foo' is weird, but okay
        "{version:2, packages:{foo:{}}}",
        // Basic package with minimum info:
        "{version:2, packages:{foo:{'1.2.3':{url: 'git+http://example.com#master'}}}}",
        // Minimal auto-lib:
        "{version:2, packages:{foo:{'1.2.3':{url: 'git+http://example.com?lm=a/b#master'}}}}",
        // Empty transforms:
        R"(
            {
                version: 2,
                packages: {foo: {'1.2.3': {
                    url: 'git+http://example.com#master',
                    transform: [],
                }}}
            }
        )",
        // Basic transform:
        R"(
            {
                version: 2,
                packages: {foo: {'1.2.3': {
                    url: 'git+http://example.com#master',
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
        version: 2,
        packages: {
            foo: {
                '1.2.3': {
                    url: 'git+http://example.com?lm=a/b#master',
                }
            }
        }
    })");
    REQUIRE(pkgs.size() == 1);
    CHECK(pkgs[0].ident.name == "foo");
    CHECK(pkgs[0].ident.to_string() == "foo@1.2.3");
    CHECK(std::holds_alternative<dds::git_remote_listing>(pkgs[0].remote));

    auto git = std::get<dds::git_remote_listing>(pkgs[0].remote);
    CHECK(git.url == "http://example.com");
    CHECK(git.ref == "master");
    REQUIRE(git.auto_lib);
    CHECK(git.auto_lib->namespace_ == "a");
    CHECK(git.auto_lib->name == "b");
}
