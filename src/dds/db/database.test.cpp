#include <dds/db/database.hpp>

#include <catch2/catch.hpp>

using namespace std::literals;

TEST_CASE("Create a database") { auto db = dds::database::open(":memory:"s); }
TEST_CASE("Read an absent file's mtime") {
    auto db        = dds::database::open(":memory:"s);
    auto mtime_opt = db.last_mtime_of("bad/file/path");
    CHECK_FALSE(mtime_opt.has_value());
}

TEST_CASE("Record a file") {
    auto db = dds::database::open(":memory:"s);
    auto time = dds::fs::file_time_type::clock::now();
    db.store_mtime("file/something", time);
    auto mtime_opt = db.last_mtime_of("file/something");
    REQUIRE(mtime_opt.has_value());
    CHECK(mtime_opt == time);
}
