#include "./migrate.hpp"

#include "./db.hpp"

#include <catch2/catch.hpp>

using namespace neo::sqlite3::literals;

struct empty_database {
    bpt::unique_database db = std::move(bpt::unique_database::open(":memory:").value());
};

TEST_CASE_METHOD(empty_database, "Run some simple migrations") {
    bpt::apply_db_migrations(  //
        db,
        "test_meta",
        [](auto& db) {
            db.exec_script(R"(
                CREATE TABLE foo (bar TEXT);
                CREATE TABLE baz (quux INTEGER);
            )"_sql);
        })
        .value();
    auto version = bpt::get_migration_version(db, "test_meta");
    REQUIRE(version);
    CHECK(*version == 1);
    db.exec_script(R"(
        INSERT INTO foo VALUES ('I am a string');
        INSERT INTO baz VALUES (42);
    )"_sql);
}
