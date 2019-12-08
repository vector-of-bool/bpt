#include <dds/catalog/catalog.hpp>

#include <catch2/catch.hpp>

using namespace std::literals;

TEST_CASE("Create a simple database") {
    // Just create and run migrations on an in-memory database
    auto repo = dds::catalog::open(":memory:"s);
}

class catalog_test_case {
public:
    dds::catalog db = dds::catalog::open(":memory:"s);
};

TEST_CASE_METHOD(catalog_test_case, "Store a simple package") {
    db.store(dds::package_info{
        dds::package_id("foo", semver::version::parse("1.2.3")),
        {},
        dds::git_remote_listing{"http://example.com", "master", std::nullopt},
    });
    CHECK_THROWS(db.store(dds::package_info{
        dds::package_id("foo", semver::version::parse("1.2.3")),
        {},
        dds::git_remote_listing{"http://example.com", "master", std::nullopt},
    }));

    auto pkgs = db.by_name("foo");
    REQUIRE(pkgs.size() == 1);
}

TEST_CASE_METHOD(catalog_test_case, "Package requirements") {
    db.store(dds::package_info{
        dds::package_id{"foo", semver::version::parse("1.2.3")},
        {
            {"bar", {semver::version::parse("1.2.3"), semver::version::parse("1.4.0")}},
            {"baz", {semver::version::parse("5.3.0"), semver::version::parse("6.0.0")}},
        },
        dds::git_remote_listing{"http://example.com", "master", std::nullopt},
    });
    auto pkgs = db.by_name("foo");
    REQUIRE(pkgs.size() == 1);
    CHECK(pkgs[0].name == "foo");
    auto deps = db.dependencies_of(pkgs[0]);
    CHECK(deps.size() == 2);
    CHECK(deps[0].name == "bar");
    CHECK(deps[1].name == "baz");
}

TEST_CASE_METHOD(catalog_test_case, "Parse JSON repo") {
    db.import_json_str(R"({
        "version": 1,
        "packages": {
            "foo": {
                "1.2.3": {
                    "depends": {
                        "bar": "~4.2.1"
                    },
                    "git": {
                        "url": "http://example.com",
                        "ref": "master"
                    }
                }
            }
        }
    })");
    auto pkgs = db.by_name("foo");
    REQUIRE(pkgs.size() == 1);
    CHECK(pkgs[0].name == "foo");
    CHECK(pkgs[0].version == semver::version::parse("1.2.3"));
    auto deps = db.dependencies_of(pkgs[0]);
    REQUIRE(deps.size() == 1);
    CHECK(deps[0].name == "bar");
    CHECK(deps[0].versions
          == dds::version_range_set{semver::version::parse("4.2.1"),
                                    semver::version::parse("4.3.0")});
}

TEST_CASE_METHOD(catalog_test_case, "Simple solve") {
    db.import_json_str(R"({
        "version": 1,
        "packages": {
            "foo": {
                "1.2.3": {
                    "depends": {
                        "bar": "~4.2.1"
                    },
                    "git": {
                        "url": "http://example.com",
                        "ref": "master"
                    }
                }
            },
            "bar": {
                "4.2.3": {
                    "depends": {},
                    "git": {
                        "url": "http://example.com",
                        "ref": "master"
                    }
                }
            }
        }
    })");
    auto sln = db.solve_requirements({{"foo",
                                       dds::version_range_set{semver::version::parse("1.0.0"),
                                                              semver::version::parse("2.0.0")}}});
    REQUIRE(sln.size() == 2);
    CHECK(sln[0].name == "foo");
    CHECK(sln[0].version == semver::version::parse("1.2.3"));
    CHECK(sln[1].name == "bar");
    CHECK(sln[1].version == semver::version::parse("4.2.3"));
}
