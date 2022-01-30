#include "./repo.hpp"

#include <dds/crs/error.hpp>
#include <dds/dds.test.hpp>
#include <dds/error/try_catch.hpp>
#include <dds/temp.hpp>

#include <catch2/catch.hpp>
#include <neo/ranges.hpp>

#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("Init repo") {
    auto tempdir = dds::temporary_dir::create();
    auto repo
        = REQUIRES_LEAF_NOFAIL(dds::crs::repository::create(tempdir.path(), "simple-test-repo"));
    dds_leaf_try {
        dds::crs::repository::create(repo.root(), "test");
        FAIL("Expected an error, but no error occurred");
    }
    dds_leaf_catch(dds::crs::e_repo_already_init) {}
    dds_leaf_catch_all { FAIL("Unexpected failure: " << diagnostic_info); };
    CHECK(repo.name() == "simple-test-repo");
}

struct empty_repo {
    dds::temporary_dir   tempdir = dds::temporary_dir::create();
    dds::crs::repository repo    = dds::crs::repository::create(tempdir.path(), "test");
};

TEST_CASE_METHOD(empty_repo, "Import a simple packages") {
    REQUIRES_LEAF_NOFAIL(repo.import_dir(dds::testing::DATA_DIR / "simple.crs"));
    auto all = REQUIRES_LEAF_NOFAIL(repo.all_packages() | neo::to_vector);
    REQUIRE(all.size() == 1);
    auto first = all.front();
    CHECK(first.name.str == "test-pkg");
    CHECK(first.version.to_string() == "1.2.43");
    CHECKED_IF(fs::is_directory(repo.pkg_dir())) {
        CHECKED_IF(fs::is_directory(repo.pkg_dir() / "test-pkg")) {
            CHECKED_IF(fs::is_directory(repo.pkg_dir() / "test-pkg/1.2.43~1")) {
                CHECK(fs::is_regular_file(repo.pkg_dir() / "test-pkg/1.2.43~1/pkg.tgz"));
                CHECK(fs::is_regular_file(repo.pkg_dir() / "test-pkg/1.2.43~1/pkg.json"));
            }
        }
    }

    REQUIRES_LEAF_NOFAIL(repo.import_dir(dds::testing::DATA_DIR / "simple2.crs"));
    all = REQUIRES_LEAF_NOFAIL(repo.all_packages() | neo::to_vector);
    REQUIRE(all.size() == 2);
    first       = all[0];
    auto second = all[1];
    CHECK(first.name.str == "test-pkg");
    CHECK(second.name.str == "test-pkg");
    CHECK(first.version.to_string() == "1.2.43");
    CHECK(second.version.to_string() == "1.3.0");

    REQUIRES_LEAF_NOFAIL(repo.import_dir(dds::testing::DATA_DIR / "simple3.crs"));
    all = REQUIRES_LEAF_NOFAIL(repo.all_packages() | neo::to_vector);
    REQUIRE(all.size() == 3);
    auto third = all[2];
    CHECK(third.name.str == "test-pkg");
    CHECK(third.version.to_string() == "1.3.0");
    CHECK(third.pkg_revision == 2);
}
