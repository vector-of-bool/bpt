#include "./db.hpp"

#include <bpt/error/result.hpp>

#include <catch2/catch.hpp>

using namespace neo::sqlite3::literals;

TEST_CASE("Open a database") {
    auto db = dds::unique_database::open(":memory:");
    CHECK(db);
    db->exec_script(R"(
        CREATE TABLE foo (bar TEXT, baz INTEGER);
        INSERT INTO foo VALUES ('quux', 42);
    )"_sql);
    db->exec_script(R"(
        INSERT INTO foo
        SELECT * FROM foo
    )"_sql);
}
