#include "./package.hpp"

#include <dds/error/result.hpp>
#include <dds/error/try_catch.hpp>
#include <dds/util/json5/error.hpp>
#include <dds/util/parse_enum.hpp>

#include <boost/leaf.hpp>
#include <boost/leaf/context.hpp>
#include <semester/walk.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Reject bad meta informations") {
    auto [given, expect_error] = GENERATE(Catch::Generators::table<std::string, std::string>({
        {"f",
         "[json.exception.parse_error.101] parse error at line 1, column 2: syntax error while "
         "parsing value - invalid literal; last read: 'f'"},
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
         "A 'pkg_revision' integer is required"},
        {R"({
            "name": "foo",
            "version": "1.2.3",
            "pkg_revision": true,
            "crs_version": 1
        })",
         "'pkg_revision' must be an integer"},
        {R"({
            "name": "foo",
            "version": "1.2.3",
            "pkg_revision": 3.14,
            "crs_version": 1
        })",
         "'pkg_revision' must be an integer"},
        {R"({
            "name": "foo",
            "version": "1.2.3",
            "pkg_revision": 1,
            "crs_version": 1
        })",
         "A string 'namespace' is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "crs_version": 1
         })",
         "A 'libraries' array is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": {},
             "crs_version": 1
         })",
         "'libraries' must be an array of library objects"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [],
             "crs_version": 1
         })",
         "'libraries' array must be non-empty"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [12],
             "crs_version": 1
         })",
         "Each library must be a JSON object"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{}],
             "crs_version": 1
         })",
         "A library 'name' is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo"
             }],
             "crs_version": 1
         })",
         "A library 'path' is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
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
             "pkg_revision": 1,
             "namespace": "cat",
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
             "pkg_revision": 1,
             "namespace": "cat",
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
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": "."
             }],
             "crs_version": 1
         })",
         "A 'uses' list is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": []
             }],
             "crs_version": 1
         })",
         "A 'depends' list is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": {}
             }],
             "crs_version": 1
         })",
         "'depends' must be an array of dependency objects"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": [12]
             }],
             "crs_version": 1
         })",
         "Each dependency should be a JSON object"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": [{}]
             }],
             "crs_version": 1
         })",
         "A string 'name' is required for each dependency"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": [{
                     "name": "bad-name."
                 }]
             }],
             "crs_version": 1
         })",
         "Invalid name string 'bad-name.': Names must not end with a punctuation character"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": [{
                     "name": "bar"
                 }]
             }],
             "crs_version": 1
         })",
         "A 'for' is required for each dependency"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": [{
                     "name": "bar",
                     "for": "lib"
                 }]
             }],
             "crs_version": 1
         })",
         "An array 'versions' is required for each dependency"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": [{
                     "name": "bar",
                     "for": "lib",
                     "versions": 12
                 }]
             }],
             "crs_version": 1
         })",
         "Dependency 'versions' must be an array"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": [{
                     "name": "bar",
                     "for": "lib",
                     "versions": [12]
                 }]
             }],
             "crs_version": 1
         })",
         "'versions' elements must be objects"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": [{
                     "name": "bar",
                     "for": "lib",
                     "versions": [],
                     "uses": []
                 }]
             }],
             "crs_version": 1
         })",
         "A dependency's 'versions' array may not be empty"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": [{
                     "name": "bar",
                     "for": "lib",
                     "versions": [{}]
                 }]
             }],
             "crs_version": 1
         })",
         "'low' version is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": [{
                     "name": "bar",
                     "for": "lib",
                     "versions": [{
                         "low": 21
                     }]
                 }]
             }],
             "crs_version": 1
         })",
         "'low' version must be a string"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": [{
                     "name": "bar",
                     "for": "lib",
                     "versions": [{
                         "low": "1.2."
                     }]
                 }]
             }],
             "crs_version": 1
         })",
         "Invalid semantic version string '1.2.'"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": [{
                     "name": "bar",
                     "for": "lib",
                     "versions": [{
                         "low": "1.2.3"
                     }]
                 }]
             }],
             "crs_version": 1
         })",
         "'high' version is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": [{
                     "name": "bar",
                     "for": "lib",
                     "versions": [{
                         "low": "1.2.3",
                         "high": 12
                     }]
                 }]
             }],
             "crs_version": 1
         })",
         "'high' version must be a string"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": [{
                     "name": "bar",
                     "for": "lib",
                     "versions": [{
                         "low": "1.2.3",
                         "high": "1.2.4"
                     }]
                 }]
             }],
             "crs_version": 1
         })",
         "A dependency 'uses' key is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": [{
                     "name": "bar",
                     "for": "lib",
                     "versions": [{
                         "low": "1.2.3",
                         "high": "1.2.3"
                     }],
                     "uses": []
                 }]
             }],
             "crs_version": 1
         })",
         "'high' version must be strictly greater than 'low' version"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": [{
                     "name": "bar",
                     "for": "lib",
                     "versions": [{
                         "low": "1.2.3",
                         "high": "1.2.3"
                     }],
                     "uses": [12]
                 }]
             }],
             "crs_version": 1
         })",
         "Each 'uses' item must be a usage string"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": [{
                     "name": "bar",
                     "for": "lib",
                     "versions": [{
                         "low": "1.2.3",
                         "high": "1.2.3"
                     }],
                     "uses": ["baz/quux"]
                 }]
             }],
             "crs_version": 1
         })",
         "Invalid name string 'baz/quux'"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": [{
                     "name": "bar",
                     "for": "lib",
                     "versions": [{
                         "low": "1.2.3",
                         "high": "1.2.3"
                     }],
                     "uses": []
                 }]
             }]
         })",
         "A 'crs_version' integer is required"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": [{
                     "name": "bar",
                     "for": "lib",
                     "versions": [{
                         "low": "1.2.3",
                         "high": "1.2.3"
                     }],
                     "uses": []
                 }]
             }],
             "crs_version": true
         })",
         "Only 'crs_version' == 1 is supported"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": [{
                     "name": "bar",
                     "for": "lib",
                     "versions": [{
                         "low": "1.2.3",
                         "high": "1.2.3"
                     }],
                     "uses": []
                 }]
             }],
             "crs_version": 1.4
         })",
         "Only 'crs_version' == 1 is supported"},
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": [{
                     "name": "bar",
                     "for": "lib",
                     "versions": [{
                         "low": "1.2.3",
                         "high": "1.2.3"
                     }],
                     "uses": []
                 }]
             }],
             "crs_version": 3
         })",
         "Only 'crs_version' == 1 is supported"},
    }));
    INFO("Parsing data: " << given);
    CAPTURE(expect_error);
    dds_leaf_try {
        dds::crs::package_info::from_json_str(given);
        FAIL("Expected a failure, but no failure occurred");
    }
    dds_leaf_catch(dds::e_json_parse_error                     err,
                   dds::crs::e_given_meta_json_str const*      json_str,
                   boost::leaf::verbose_diagnostic_info const& diag_info) {
        CAPTURE(diag_info);
        CHECKED_IF(json_str) { CHECK(json_str->value == given); }
        CHECK_THAT(err.value, Catch::Matchers::Contains(expect_error));
    }
    dds_leaf_catch(dds::crs::e_given_meta_json_str const*      error_str,
                   dds::crs::e_given_meta_json_data const*     error_data,
                   dds::crs::e_invalid_meta_data               e,
                   boost::leaf::verbose_diagnostic_info const& diag_info) {
        CAPTURE(diag_info);
        CHECKED_IF(error_str) { CHECK(error_str->value == given); }
        CHECK(error_data);
        CHECK_THAT(e.value, Catch::Matchers::Contains(expect_error));
    }
    dds_leaf_catch_all {  //
        FAIL_CHECK("Unexpected error: " << diagnostic_info);
    };

    dds_leaf_try {
        dds::crs::package_info::from_json_str(R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": [{
                     "name": "bar",
                     "for": "meow"
                 }]
             }],
             "crs_version": 1
         })");
        FAIL_CHECK("Expected an error, but non occurred");
    }
    dds_leaf_catch(dds::e_invalid_enum<dds::crs::usage_kind>,
                   dds::e_invalid_enum_str bad_str,
                   dds::e_enum_options     opts) {
        CHECK(bad_str.value == "meow");
        CHECK(opts.value == R"("lib", "test", "app")");
    }
    dds_leaf_catch_all { FAIL_CHECK("Unexpected error: " << diagnostic_info); };
}

TEST_CASE("Check some valid meta JSON") {
    const auto given = GENERATE(R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": []
             }],
             "crs_version": 1
         })");
    REQUIRE_NOTHROW(dds::crs::package_info::from_json_str(given));
}

auto mk_name = [](std::string_view s) { return dds::name::from_string(s).value(); };

#ifndef _MSC_VER  // MSVC struggles with compiling this test
TEST_CASE("Check parse results") {
    using pkg_meta             = dds::crs::package_info;
    using lib_meta             = dds::crs::library_info;
    using dependency           = dds::crs::dependency;
    const auto [given, expect] = GENERATE(Catch::Generators::table<std::string, pkg_meta>({
        {R"({
             "name": "foo",
             "version": "1.2.3",
             "pkg_revision": 1,
             "namespace": "cat",
             "libraries": [{
                 "name": "foo",
                 "path": ".",
                 "uses": [],
                 "depends": [{
                     "name": "bar",
                     "for": "lib",
                     "versions": [{
                         "low": "1.0.0",
                         "high": "1.5.1"
                     }],
                     "uses": ["bar"]
                 }]
             }],
             "crs_version": 1
         })",
         pkg_meta{
             .name         = mk_name("foo"),
             .namespace_   = mk_name("cat"),
             .version      = semver::version::parse("1.2.3"),
             .pkg_revision = 1,
             .libraries    = {lib_meta{
                 .name         = mk_name("foo"),
                 .path         = ".",
                 .intra_uses   = {},
                 .dependencies = {dependency{
                     .name = mk_name("bar"),
                     .acceptable_versions
                     = dds::crs::version_range_set{semver::version::parse("1.0.0"),
                                                   semver::version::parse("1.5.1")},
                     .kind = dds::crs::usage_kind::lib,
                     .uses = dds::crs::explicit_uses_list{{dds::name{"baz"}}},
                 }},
             }},
             .extra        = {},
         }},
    }));

    auto meta = dds_leaf_try { return dds::crs::package_info::from_json_str(given); }
    dds_leaf_catch_all->dds::noreturn_t {
        FAIL("Unexpected error: " << diagnostic_info);
        std::terminate();
    };

    CHECK(meta.name == expect.name);
    CHECK(meta.namespace_ == expect.namespace_);
    CHECK(meta.version == expect.version);
    CHECK(meta.pkg_revision == expect.pkg_revision);
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