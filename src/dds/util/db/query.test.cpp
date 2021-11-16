#include "./query.hpp"

#include <dds/error/result.hpp>

#include <catch2/catch.hpp>

using namespace neo::sqlite3::literals;

TEST_CASE("Database operations") {
    auto db = dds::unique_database::open(":memory:");
    CHECK(db);
    db->exec_script(R"(
        CREATE TABLE foo (bar TEXT, baz INTEGER);
        INSERT INTO foo VALUES ('quux', 42);
    )"_sql);

    auto tups     = dds::db_query<std::string, int>(*db, "SELECT * FROM foo"_sql);
    auto [str, v] = *tups.begin();
    CHECK(str == "quux");
    CHECK(v == 42);

    auto single = dds::db_cell<std::string>(*db, "SELECT bar FROM foo LIMIT 1"_sql);
    CHECK(single == "quux");
}
