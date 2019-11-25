#include <dds/db/database.hpp>

#include <catch2/catch.hpp>

using namespace std::literals;

TEST_CASE("Create a database") { auto db = dds::database::open(":memory:"s); }
