#include "./cache_db.hpp"

#include <dds/crs/meta.hpp>
#include <dds/dds.test.hpp>
#include <dds/error/result.hpp>

#include <boost/leaf.hpp>
#include <catch2/catch.hpp>

using namespace neo::sqlite3::literals;

TEST_CASE("Open a cache") {
    auto db  = REQUIRES_LEAF_NOFAIL(dds::unique_database::open(":memory:").value());
    auto ldr = REQUIRES_LEAF_NOFAIL(dds::crs::cache_db::open(db));
    (void)ldr;
}

struct empty_loader {
    dds::unique_database db    = dds::unique_database::open(":memory:").value();
    dds::crs::cache_db   cache = dds::crs::cache_db::open(db);
};

TEST_CASE_METHOD(empty_loader, "Get a non-existent remote") {
    auto url   = neo::url::parse("http://example.com");
    auto entry = REQUIRES_LEAF_NOFAIL(cache.get_remote(url));
    CHECK_FALSE(entry.has_value());
}
