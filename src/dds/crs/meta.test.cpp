#include "./meta.hpp"

#include <boost/leaf/context.hpp>
#include <boost/leaf/handle_exception.hpp>
#include <dds/error/result.hpp>
#include <semester/walk.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Reject bad meta informations") {
    auto [given, expect_error] = GENERATE(Catch::Generators::table<std::string, std::string>({
        {"f", "An object key identifier is not a valid value."},
        {"{\"crs_version\": 1}", "A string 'name' is required"},
        {R"({"crs_version": 1, "name": "foo"})", "A 'version' string is required"},
        {R"({"crs_version": 1, "name": "foo."})",
         "Invalid name string 'foo.': Names must not end with a punctuation character"},
        {R"({"crs_version": 1, "name": "foo", "version": "bleh"})",
         "Invalid semantic version string 'bleh'"},
        {R"({
            "name": "foo",
            "version": "1.2.3",
            "crs_version": 1
        })",
         "A 'meta_version' integer is required"},
        {R"({
            "name": "foo",
            "version": "1.2.3",
            "meta_version": true,
            "crs_version": 1
        })",
         "'meta_version' must be an integer"},
        {R"({
            "name": "foo",
            "version": "1.2.3",
            "meta_version": 3.14,
            "crs_version": 1
        })",
         "'meta_version' must be an integer"},
        {R"({
            "name": "foo",
            "version": "1.2.3",
            "meta_version": 1,
            "crs_version": 1
        })",
         "A string 'namespace' is required"},
        {R"({
            "name": "foo",
            "version": "1.2.3",
            "meta_version": 1,
            "namespace": "cat",
            "crs_version": 1
        })",
         "A 'depends' list is required"},
        {R"({
            "name": "foo",
            "version": "1.2.3",
            "meta_version": 1,
            "namespace": "cat",
            "depends": {},
            "crs_version": 1
        })",
         "'depends' must be an array of dependency objects"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [],
            "crs_version": 1
         })",
         "A 'libraries' array is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [],
             "libraries": {},
            "crs_version": 1
         })",
         "'libraries' must be an array of library objects"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [12],
             "libraries": [],
             "crs_version": 1
         })",
         "Each dependency should be a JSON object"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [{}],
             "libraries": [],
             "crs_version": 1
         })",
         "A string 'name' is required for each dependency"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [{
                 "name": "bad-name."
             }],
             "libraries": [],
             "crs_version": 1
         })",
         "Invalid name string 'bad-name.': Names must not end with a punctuation character"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [{
                 "name": "bar"
             }],
             "libraries": [],
             "crs_version": 1
         })",
         "An array 'versions' is required for each dependency"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [{
                 "name": "bar",
                 "versions": 12
             }],
             "libraries": [],
             "crs_version": 1
         })",
         "Dependency 'versions' must be an array"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [{
                 "name": "bar",
                 "versions": [12]
             }],
             "libraries": [],
             "crs_version": 1
         })",
         "'versions' elements must be objects"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [{
                 "name": "bar",
                 "versions": []
             }],
             "libraries": [],
             "crs_version": 1
         })",
         "A dependency's 'versions' array may not be empty"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [{
                 "name": "bar",
                 "versions": [{}]
             }],
             "libraries": [],
             "crs_version": 1
         })",
         "'low' version is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [{
                 "name": "bar",
                 "versions": [{
                     "low": 21
                 }]
             }],
             "libraries": [],
             "crs_version": 1
         })",
         "'low' version must be a string"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [{
                 "name": "bar",
                 "versions": [{
                     "low": "1.2."
                 }]
             }],
             "libraries": [],
             "crs_version": 1
         })",
         "Invalid semantic version string '1.2.'"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [{
                 "name": "bar",
                 "versions": [{
                     "low": "1.2.3"
                 }]
             }],
             "libraries": [],
             "crs_version": 1
         })",
         "'high' version is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [{
                 "name": "bar",
                 "versions": [{
                     "low": "1.2.3",
                     "high": 12
                 }]
             }],
             "libraries": [],
             "crs_version": 1
         })",
         "'high' version must be a string"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [{
                 "name": "bar",
                 "versions": [{
                     "low": "1.2.3",
                     "high": "1.2."
                 }]
             }],
             "libraries": [],
             "crs_version": 1
         })",
         "Invalid semantic version string '1.2.'"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [{
                 "name": "bar",
                 "versions": [{
                     "low": "1.2.3",
                     "high": "1.2.3"
                 }]
             }],
             "libraries": [],
             "crs_version": 1
         })",
         "'high' version must be strictly greater than 'low' version"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [],
             "libraries": [],
             "crs_version": 1
         })",
         "'libraries' array must be non-empty"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [],
             "libraries": [12],
             "crs_version": 1
         })",
         "Each library must be a JSON object"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [],
             "libraries": [{}],
             "crs_version": 1
         })",
         "A library 'name' is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [],
             "libraries": [{
                 "name": "foo"
             }],
             "crs_version": 1
         })",
         "A library 'path' is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [],
             "libraries": [{
                 "name": "foo",
                 "path": 12
             }],
             "crs_version": 1
         })",
         "Library 'path' must be a string"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [],
             "libraries": [{
                 "name": "foo",
                 "path": "/foo/bar"
             }],
             "crs_version": 1
         })",
         "Library path [/foo/bar] must be a relative path"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [],
             "libraries": [{
                 "name": "foo",
                 "path": "../bar"
             }],
             "crs_version": 1
         })",
         "Library path [../bar] must not reach outside of the distribution directory."},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [],
             "libraries": [{
                 "name": "foo",
                 "path": "."
             }],
             "crs_version": 1
         })",
         "A library 'uses' key is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [],
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [12]
             }],
             "crs_version": 1
         })",
         "Each 'uses' item must be a JSON object"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [],
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [{}]
             }],
             "crs_version": 1
         })",
         "A usage 'for' is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [],
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [{
                     "for": "dog"
                 }]
             }],
             "crs_version": 1
         })",
         "Invalid 'uses' 'for' string 'dog'"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [],
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [{
                     "for": "lib"
                 }]
             }],
             "crs_version": 1
         })",
         "A 'lib' key is required in each 'uses' object"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [],
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [{
                     "for": "lib",
                     "lib": 12
                 }]
             }],
             "crs_version": 1
         })",
         "'lib' must be a usage string"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [],
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [{
                     "for": "lib",
                     "lib": "bar"
                 }]
             }],
             "crs_version": 1
         })",
         "Invalid usage string 'bar'"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [],
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [{
                     "for": "lib",
                     "lib": "bar/baz"
                 }]
             }]
         })",
         "A 'crs_version' integer is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [],
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [{
                     "for": "lib",
                     "lib": "bar/baz"
                 }]
             }],
             "crs_version": true
         })",
         "Only 'crs_version' == 1 is supported"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [],
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [{
                     "for": "lib",
                     "lib": "bar/baz"
                 }]
             }],
             "crs_version": 1.4
         })",
         "Only 'crs_version' == 1 is supported"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [],
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [{
                     "for": "lib",
                     "lib": "bar/baz"
                 }]
             }],
             "crs_version": 3
         })",
         "Only 'crs_version' == 1 is supported"},
    }));
    INFO("Parsing data: " << given);
    boost::leaf::context<dds::crs::e_invalid_meta_data> err_ctx;
    err_ctx.activate();
    auto result = dds::crs::package_meta::from_json_str(given);
    err_ctx.deactivate();
    CHECKED_IF(!result) {
        err_ctx.handle_error<void>(
            result.error(),
            [&](dds::crs::e_invalid_meta_data e) {
                CHECK_THAT(e.message, Catch::Matchers::EndsWith(expect_error));
            },
            [] { FAIL_CHECK("Bad error"); });
    }
}

TEST_CASE("Check some valid meta JSON") {
    const auto given = GENERATE(R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [],
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [{
                     "for": "lib",
                     "lib": "bar/baz"
                 }]
             }],
             "crs_version": 1
         })");
    auto       meta  = dds::crs::package_meta::from_json_str(given);
    CHECK(meta);
}

auto mk_name = [](std::string_view s) { return *dds::name::from_string(s); };

TEST_CASE("Check parse results") {
    using pkg_meta             = dds::crs::package_meta;
    using lib_meta             = dds::crs::library_meta;
    using usage                = dds::crs::usage;
    const auto [given, expect] = GENERATE(Catch::Generators::table<std::string, pkg_meta>({
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [],
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [{
                     "for": "lib",
                     "lib": "bar/baz"
                 }]
             }],
             "crs_version": 1
         })",
         pkg_meta{
             .name         = mk_name("foo"),
             .namespace_   = mk_name("cat"),
             .version      = semver::version::parse("1.2.3"),
             .meta_version = 1,
             .dependencies = {},
             .libraries    = {lib_meta{
                 .name = mk_name("foo"),
                 .path = ".",
                 .uses = {usage{
                     .lib  = lm::usage{"bar", "baz"},
                     .kind = dds::crs::usage_kind::lib,
                 }},
             }},
             .extra        = {},
         }},
    }));

    auto meta_ = dds::crs::package_meta::from_json_str(given);
    REQUIRE(meta_);
    auto meta = *meta_;
    CHECK(meta.name == expect.name);
    CHECK(meta.namespace_ == expect.namespace_);
    CHECK(meta.version == expect.version);
    CHECK(meta.meta_version == expect.meta_version);
    CHECK(meta.extra == expect.extra);
    CHECKED_IF(meta.dependencies.size() == expect.dependencies.size()) {
        auto res_dep_it = meta.dependencies.cbegin();
        auto exp_dep_it = meta.dependencies.cbegin();
        for (; res_dep_it != meta.dependencies.cbegin(); ++res_dep_it, ++exp_dep_it) {
            CHECK(res_dep_it->name == exp_dep_it->name);
            CHECK(res_dep_it->acceptable_versions == exp_dep_it->acceptable_versions);
        }
    }
    CHECKED_IF(meta.libraries.size() == expect.libraries.size()) {
        auto res_lib_it = meta.libraries.cbegin();
        auto exp_lib_it = expect.libraries.cbegin();
        for (; res_lib_it != meta.libraries.cend(); ++res_lib_it, ++exp_lib_it) {
            CHECK(res_lib_it->name == exp_lib_it->name);
            CHECK(res_lib_it->path == exp_lib_it->path);
            CHECKED_IF(res_lib_it->uses.size() == exp_lib_it->uses.size()) {
                auto res_use_it = res_lib_it->uses.cbegin();
                auto exp_use_it = exp_lib_it->uses.cbegin();
                for (; res_use_it != res_lib_it->uses.cend(); ++res_use_it, ++exp_use_it) {
                    CHECK(res_use_it->kind == exp_use_it->kind);
                    CHECK(res_use_it->lib == exp_use_it->lib);
                }
            }
        }
    }
    CHECK_NOTHROW(meta.to_json());
}
