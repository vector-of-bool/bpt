#include "./query.hpp"

#include <bpt/error/result.hpp>

#include <catch2/catch.hpp>

using namespace neo::sqlite3::literals;

TEST_CASE("Database operations") {
    auto db = dds::unique_database::open(":memory:");
    CHECK(db);
    db->exec_script(R"(
        CREATE TABLE foo (bar TEXT, baz INTEGER);
        INSERT INTO foo VALUES ('quux', 42);
    )"_sql);

    auto& get_row = db->prepare("SELECT * FROM foo"_sql);
    auto  tups    = dds::db_query<std::string, int>(get_row);
    auto [str, v] = *tups.begin();
    CHECK(str == "quux");
    CHECK(v == 42);
    get_row.reset();

    auto [str2, v2] = dds::db_single<std::string, int>(get_row).value();
    CHECK(str2 == "quux");
    CHECK(v2 == 42);
    get_row.reset();

    auto single = dds::db_cell<std::string>(db->prepare("SELECT bar FROM foo LIMIT 1"_sql));
    REQUIRE(single);
    CHECK(*single == "quux");
}
