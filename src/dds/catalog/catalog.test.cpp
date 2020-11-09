#include <dds/catalog/catalog.hpp>

#include <catch2/catch.hpp>

using namespace std::literals;

TEST_CASE("Create a simple database") {
    // Just create and run migrations on an in-memory database
    auto repo = dds::catalog::open(":memory:"s);
}

TEST_CASE("Open a catalog in a non-ascii path") {
    ::setlocale(LC_ALL, ".utf8");
    auto THIS_DIR = dds::fs::canonical(__FILE__).parent_path();
    auto BUILD_DIR
        = (THIS_DIR.parent_path().parent_path().parent_path() / "_build").lexically_normal();
    auto subdir = BUILD_DIR / "Ю́рий Алексе́евич Гага́рин";
    dds::fs::remove_all(subdir);
    dds::catalog::open(subdir / "test.db");
    dds::fs::remove_all(subdir);
}

class catalog_test_case {
public:
    dds::catalog db = dds::catalog::open(":memory:"s);
};

TEST_CASE_METHOD(catalog_test_case, "Store a simple package") {
    db.store(dds::package_info{
        dds::package_id("foo", semver::version::parse("1.2.3")),
        {},
        "example",
        dds::git_remote_listing{std::nullopt, {}, "git+http://example.com", "master"},
    });

    auto pkgs = db.by_name("foo");
    REQUIRE(pkgs.size() == 1);
    CHECK(pkgs[0].name == "foo");
    CHECK(pkgs[0].version == semver::version::parse("1.2.3"));
    auto info = db.get(pkgs[0]);
    REQUIRE(info);
    CHECK(info->ident == pkgs[0]);
    CHECK(info->deps.empty());
    CHECK(std::holds_alternative<dds::git_remote_listing>(info->remote));
    CHECK(std::get<dds::git_remote_listing>(info->remote).ref == "master");

    // Update the entry with a new git remote ref
    CHECK_NOTHROW(db.store(dds::package_info{
        dds::package_id("foo", semver::version::parse("1.2.3")),
        {},
        "example",
        dds::git_remote_listing{std::nullopt, {}, "git+http://example.com", "develop"},
    }));
    // The previous pkg_id is still a valid lookup key
    info = db.get(pkgs[0]);
    REQUIRE(info);
    CHECK(std::get<dds::git_remote_listing>(info->remote).ref == "develop");
}

TEST_CASE_METHOD(catalog_test_case, "Package requirements") {
    db.store(dds::package_info{
        dds::package_id{"foo", semver::version::parse("1.2.3")},
        {
            {"bar", {semver::version::parse("1.2.3"), semver::version::parse("1.4.0")}},
            {"baz", {semver::version::parse("5.3.0"), semver::version::parse("6.0.0")}},
        },
        "example",
        dds::git_remote_listing{std::nullopt, {}, "git+http://example.com", "master"},
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
        "version": 2,
        "packages": {
            "foo": {
                "1.2.3": {
                    "depends": [
                        "bar~4.2.1"
                    ],
                    url: "git+http://example.com#master"
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
