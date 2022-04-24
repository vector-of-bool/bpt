#include "./package.hpp"

#include <bpt/error/result.hpp>
#include <bpt/error/try_catch.hpp>
#include <bpt/util/json5/error.hpp>
#include <bpt/util/parse_enum.hpp>

#include <boost/leaf.hpp>
#include <boost/leaf/context.hpp>
#include <semester/walk.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Reject bad meta informations") {
    auto [given, expect_error] = GENERATE(Catch::Generators::table<std::string, std::string>({
        {"f",
         "[json.exception.parse_error.101] parse error at line 1, column 2: syntax error while "
         "parsing value - invalid literal; last read: 'f'"},
        {"{\"schema-version\": 1}", "A string 'name' is required"},
        {R"({"schema-version": 1, "name": "foo"})", "A 'version' string is required"},
        {R"({"schema-version": 1, "name": "foo."})",
         "Invalid name string 'foo.': Names must not end with a punctuation character"},
        {R"({"schema-version": 1, "name": "foo", "version": "bleh"})",
         "Invalid semantic version string 'bleh'"},
        {R"({
            "name": "foo",
            "version": "1.2.3",
            "schema-version": 1
        })",
         "A 'pkg-version' integer is required"},
        {R"({
            "name": "foo",
            "version": "1.2.3",
            "pkg-version": true,
            "schema-version": 1
        })",
         "'pkg-version' must be an integer"},
        {R"({
            "name": "foo",
            "version": "1.2.3",
            "pkg-version": 3.14,
            "schema-version": 1
        })",
         "'pkg-version' must be an integer"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "schema-version": 1
         })",
         "A 'libraries' array is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": {},
             "schema-version": 1
         })",
         "'libraries' must be an array of library objects"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [],
             "schema-version": 1
         })",
         "'libraries' array must be non-empty"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [12],
             "schema-version": 1
         })",
         "Each library must be a JSON object"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{}],
             "schema-version": 1
         })",
         "A library 'name' string is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo"
             }],
             "schema-version": 1
         })",
         "A library 'path' string is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": 12
             }],
             "schema-version": 1
         })",
         "Library 'path' must be a string"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": "/foo/bar"
             }],
             "schema-version": 1
         })",
         "Library path [/foo/bar] must be a relative path"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": "../bar"
             }],
             "schema-version": 1
         })",
         "Library path [../bar] must not reach outside of the distribution directory."},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "test-using": []
             }],
             "schema-version": 1
         })",
         "A 'using' array is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "using": []
             }],
             "schema-version": 1
         })",
         "A 'test-using' array is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "test-using": [],
                 "using": []
             }],
             "schema-version": 1
         })",
         "A 'dependencies' array is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "using": [],
                 "dependencies": {}
             }],
             "schema-version": 1
         })",
         "'dependencies' must be an array of dependency objects"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "using": [],
                 "dependencies": [12]
             }],
             "schema-version": 1
         })",
         "Each dependency should be a JSON object"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "test-using": [],
                 "using": [],
                 "test-dependencies": [],
                 "dependencies": [{}]
             }],
             "schema-version": 1
         })",
         "A string 'name' is required for each dependency"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "test-using": [],
                 "using": [],
                 "test-dependencies": [],
                 "dependencies": [{
                     "name": "bad-name."
                 }]
             }],
             "schema-version": 1
         })",
         "Invalid name string 'bad-name.': Names must not end with a punctuation character"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "test-using": [],
                 "using": [],
                 "dependencies": [{
                     "name": "bar",
                     "versions": [{
                         "low": "1.2.3",
                         "high": "1.2.4"
                     }],
                     "using": ["foo"]
                 }]
             }],
             "schema-version": 1
         })",
         "A 'test-dependencies' array is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "test-using": [],
                 "using": [],
                 "test-dependencies": [],
                 "dependencies": [{
                     "name": "bar"
                 }]
             }],
             "schema-version": 1
         })",
         "A 'versions' array is required for each dependency"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "test-using": [],
                 "using": [],
                 "test-dependencies": [],
                 "dependencies": [{
                     "name": "bar",
                     "versions": 12
                 }]
             }],
             "schema-version": 1
         })",
         "Dependency 'versions' must be an array"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "test-using": [],
                 "using": [],
                 "test-dependencies": [],
                 "dependencies": [{
                     "name": "bar",
                     "versions": [12]
                 }]
             }],
             "schema-version": 1
         })",
         "'versions' elements must be objects"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "test-using": [],
                 "using": [],
                 "test-dependencies": [],
                 "dependencies": [{
                     "name": "bar",
                     "versions": [],
                     "using": []
                 }]
             }],
             "schema-version": 1
         })",
         "A dependency's 'versions' array may not be empty"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "test-using": [],
                 "using": [],
                 "test-dependencies": [],
                 "dependencies": [{
                     "name": "bar",
                     "versions": [{}]
                 }]
             }],
             "schema-version": 1
         })",
         "'low' version is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "test-using": [],
                 "using": [],
                 "test-dependencies": [],
                 "dependencies": [{
                     "name": "bar",
                     "versions": [{
                         "low": 21
                     }]
                 }]
             }],
             "schema-version": 1
         })",
         "'low' version must be a string"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "test-using": [],
                 "using": [],
                 "test-dependencies": [],
                 "dependencies": [{
                     "name": "bar",
                     "versions": [{
                         "low": "1.2."
                     }]
                 }]
             }],
             "schema-version": 1
         })",
         "Invalid semantic version string '1.2.'"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "test-using": [],
                 "using": [],
                 "test-dependencies": [],
                 "dependencies": [{
                     "name": "bar",
                     "versions": [{
                         "low": "1.2.3"
                     }]
                 }]
             }],
             "schema-version": 1
         })",
         "'high' version is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "test-using": [],
                 "using": [],
                 "test-dependencies": [],
                 "dependencies": [{
                     "name": "bar",
                     "versions": [{
                         "low": "1.2.3",
                         "high": 12
                     }]
                 }]
             }],
             "schema-version": 1
         })",
         "'high' version must be a string"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "test-using": [],
                 "using": [],
                 "test-dependencies": [],
                 "dependencies": [{
                     "name": "bar",
                     "versions": [{
                         "low": "1.2.3",
                         "high": "1.2.4"
                     }]
                 }]
             }],
             "schema-version": 1
         })",
         "A dependency 'using' key is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "test-using": [],
                 "using": [],
                 "test-dependencies": [],
                 "dependencies": [{
                     "name": "bar",
                     "versions": [{
                         "low": "1.2.3",
                         "high": "1.2.3"
                     }],
                     "using": []
                 }]
             }],
             "schema-version": 1
         })",
         "'high' version must be strictly greater than 'low' version"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "test-using": [],
                 "using": [],
                 "test-dependencies": [],
                 "dependencies": [{
                     "name": "bar",
                     "versions": [{
                         "low": "1.2.3",
                         "high": "1.2.3"
                     }],
                     "using": [12]
                 }]
             }],
             "schema-version": 1
         })",
         "Each 'using' item must be a usage string"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "test-using": [],
                 "using": [],
                 "test-dependencies": [],
                 "dependencies": [{
                     "name": "bar",
                     "versions": [{
                         "low": "1.2.3",
                         "high": "1.2.3"
                     }],
                     "using": ["baz/quux"]
                 }]
             }],
             "schema-version": 1
         })",
         "Invalid name string 'baz/quux'"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "test-using": [],
                 "using": [],
                 "test-dependencies": [],
                 "dependencies": [{
                     "name": "bar",
                     "versions": [{
                         "low": "1.2.3",
                         "high": "1.2.3"
                     }],
                     "using": []
                 }]
             }]
         })",
         "A 'schema-version' integer is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "test-using": [],
                 "using": [],
                 "test-dependencies": [],
                 "dependencies": [{
                     "name": "bar",
                     "versions": [{
                         "low": "1.2.3",
                         "high": "1.2.3"
                     }],
                     "using": []
                 }]
             }],
             "schema-version": true
         })",
         "Only 'schema-version' == 1 is supported"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "test-using": [],
                 "using": [],
                 "test-dependencies": [],
                 "dependencies": [{
                     "name": "bar",
                     "versions": [{
                         "low": "1.2.3",
                         "high": "1.2.3"
                     }],
                     "using": []
                 }]
             }],
             "schema-version": 1.4
         })",
         "Only 'schema-version' == 1 is supported"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "test-using": [],
                 "using": [],
                 "test-dependencies": [],
                 "dependencies": [{
                     "name": "bar",
                     "versions": [{
                         "low": "1.2.3",
                         "high": "1.2.3"
                     }],
                     "using": []
                 }]
             }],
             "schema-version": 3
         })",
         "Only 'schema-version' == 1 is supported"},
    }));
    INFO("Parsing data: " << given);
    CAPTURE(expect_error);
    bpt_leaf_try {
        bpt::crs::package_info::from_json_str(given);
        FAIL("Expected a failure, but no failure occurred");
    }
    bpt_leaf_catch(bpt::e_json_parse_error                     err,
                   bpt::crs::e_given_meta_json_str const*      json_str,
                   boost::leaf::verbose_diagnostic_info const& diag_info) {
        CAPTURE(diag_info);
        CHECKED_IF(json_str) { CHECK(json_str->value == given); }
        CHECK_THAT(err.value, Catch::Matchers::Contains(expect_error));
    }
    bpt_leaf_catch(bpt::crs::e_given_meta_json_str const*      error_str,
                   bpt::crs::e_given_meta_json_data const*     error_data,
                   bpt::crs::e_invalid_meta_data               e,
                   boost::leaf::verbose_diagnostic_info const& diag_info) {
        CAPTURE(diag_info);
        CHECKED_IF(error_str) { CHECK(error_str->value == given); }
        CHECK(error_data);
        CHECK_THAT(e.value, Catch::Matchers::Contains(expect_error));
    }
    bpt_leaf_catch_all {  //
        FAIL_CHECK("Unexpected error: " << diagnostic_info);
    };
}

TEST_CASE("Check some valid meta JSON") {
    const auto given = GENERATE(R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "test-using": [],
                 "using": [],
                 "test-dependencies": [],
                 "dependencies": []
             }],
             "schema-version": 1
         })");
    REQUIRE_NOTHROW(bpt::crs::package_info::from_json_str(given));
}

auto mk_name = [](std::string_view s) { return bpt::name::from_string(s).value(); };

#ifndef _MSC_VER  // MSVC struggles with compiling this test
TEST_CASE("Check parse results") {
    using pkg_meta   = bpt::crs::package_info;
    using lib_meta   = bpt::crs::library_info;
    using dependency = bpt::crs::dependency;
    const auto [given, expect] = GENERATE(Catch::Generators::table<std::string, pkg_meta>({
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg-version": 1,
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "test-using": [],
                 "using": [],
                 "test-dependencies": [],
                 "dependencies": [{
                     "name": "bar",
                     "versions": [{
                         "low": "1.0.0",
                         "high": "1.5.1"
                     }],
                     "using": ["bar"]
                 }],
                 "test-dependencies": []
             }],
             "schema-version": 1
         })",
         pkg_meta{
             .id=bpt::crs::pkg_id{
                .name         = mk_name("foo"),
                .version      = semver::version::parse("1.2.3"),
                .revision = 1,
             },
             .libraries    = {lib_meta{
                 .name         = mk_name("foo"),
                 .path         = ".",
                 .intra_using   = {},
                 .intra_test_using = {},
                 .dependencies = {dependency{
                     .name = mk_name("bar"),
                     .acceptable_versions
                     = bpt::crs::version_range_set{semver::version::parse("1.0.0"),
                                                   semver::version::parse("1.5.1")},
                     .uses = {bpt::name{"baz"}},
                 }},
                 .test_dependencies ={},
             }},
             .extra        = {},
             .meta         = {},
         }},
    }));

    auto meta = bpt_leaf_try { return bpt::crs::package_info::from_json_str(given); }
    bpt_leaf_catch_all->bpt::noreturn_t {
        FAIL("Unexpected error: " << diagnostic_info);
        std::terminate();
    };

    CHECK(meta.id == expect.id);
    CHECK(meta.extra == expect.extra);
    CHECKED_IF(meta.libraries.size() == expect.libraries.size()) {
        auto res_lib_it = meta.libraries.cbegin();
        auto exp_lib_it = expect.libraries.cbegin();
        for (; res_lib_it != meta.libraries.cend(); ++res_lib_it, ++exp_lib_it) {
            CHECK(res_lib_it->name == exp_lib_it->name);
            CHECK(res_lib_it->path == exp_lib_it->path);
            CHECKED_IF(res_lib_it->dependencies.size() == exp_lib_it->dependencies.size()) {
                auto res_dep_it = res_lib_it->dependencies.cbegin();
                auto exp_dep_it = res_lib_it->dependencies.cbegin();
                for (; res_dep_it != res_lib_it->dependencies.cbegin();
                     ++res_dep_it, ++exp_dep_it) {
                    CHECK(res_dep_it->name == exp_dep_it->name);
                    CHECK(res_dep_it->acceptable_versions == exp_dep_it->acceptable_versions);
                    CHECK(res_dep_it->uses == exp_dep_it->uses);
                }
            }
        }
    }
    CHECK_NOTHROW(meta.to_json());
}
#endif  // _MSC_VER