#include "./cache_db.hpp"

#include <dds/crs/meta.hpp>
#include <dds/dds.test.hpp>
#include <dds/error/handle.hpp>
#include <dds/error/try_catch.hpp>
#include <dds/util/http/error.hpp>
#include <dds/util/http/response.hpp>

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

TEST_CASE_METHOD(empty_loader, "Sync an invalid repo") {
    auto url = neo::url::parse("http://example.com");
    dds_leaf_try {
        cache.sync_repo(url);
        FAIL_CHECK("Expected an error to occur");
    }
    dds_leaf_catch(const dds::http_error&  exc,
                   dds::http_response_info resp,
                   dds::matchv<dds::e_http_status{404}>) {
        CHECK(exc.status_code() == 404);
        CHECK(resp.status == 404);
    }
    dds_leaf_catch_all { FAIL_CHECK("Unhandled error: " << diagnostic_info); };
}
