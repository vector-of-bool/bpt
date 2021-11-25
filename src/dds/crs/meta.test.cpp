#include "./meta.hpp"

#include <dds/error/result.hpp>
#include <dds/error/try_catch.hpp>
#include <dds/util/json5/parse.hpp>

#include <boost/leaf.hpp>
#include <boost/leaf/context.hpp>
#include <semester/walk.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Reject bad meta informations") {
    auto [given, expect_error] = GENERATE(Catch::Generators::table<std::string, std::string>({
        {"f",
         "JSON parse error while loading CRS meta: [json.exception.parse_error.101] parse error at "
         "line 1, column 2: syntax error while parsing value - invalid literal; last read: 'f'"},
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
         "A 'for' is required for each dependency"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [{
                 "name": "bar",
                 "for": "meow"
             }],
             "libraries": [],
             "crs_version": 1
         })",
         "Invalid usage kind string 'meow' (Should be one of 'lib', 'app', or 'test')"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "meta_version": 1,
             "namespace": "cat",
             "depends": [{
                 "name": "bar",
                 "for": "lib"
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
                 "for": "lib",
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
                 "for": "lib",
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
                 "for": "lib",
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
                 "for": "lib",
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
                 "for": "lib",
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
                 "for": "lib",
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
                 "for": "lib",
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
                 "for": "lib",
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
                 "for": "lib",
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
                 "for": "lib",
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
         "Invalid usage kind string 'dog' (Should be one of 'lib', 'app', or 'test')"},
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
    CAPTURE(expect_error);
    dds_leaf_try {
        dds::crs::package_meta::from_json_str(given);
        FAIL("Expected a failure, but no failure occurred");
    }
    dds_leaf_catch(dds::e_json_parse_error,
                   dds::crs::e_given_meta_json_str const*      json_str,
                   dds::crs::e_invalid_meta_data               e,
                   boost::leaf::verbose_diagnostic_info const& diag_info) {
        CAPTURE(diag_info);
        CHECKED_IF(json_str) { CHECK(json_str->value == given); }
        CHECK_THAT(e.value, Catch::Matchers::EndsWith(expect_error));
    }
    dds_leaf_catch(dds::crs::e_given_meta_json_str const*      error_str,
                   dds::crs::e_given_meta_json_data const*     error_data,
                   dds::crs::e_invalid_meta_data               e,
                   boost::leaf::verbose_diagnostic_info const& diag_info) {
        CAPTURE(diag_info);
        CHECKED_IF(error_str) { CHECK(error_str->value == given); }
        CHECK(error_data);
        CHECK_THAT(e.value, Catch::Matchers::EndsWith(expect_error));
    }
    dds_leaf_catch_all {  //
        FAIL_CHECK("Bad error: " << diagnostic_info);
    };
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
    REQUIRE_NOTHROW(dds::crs::package_meta::from_json_str(given));
}

auto mk_name = [](std::string_view s) { return dds::name::from_string(s).value(); };

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

    auto meta = dds_leaf_try { return dds::crs::package_meta::from_json_str(given); }
    dds_leaf_catch_all->dds::noreturn_t {
        FAIL("Unexpected error: " << diagnostic_info);
        std::terminate();
    };

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
