#include "./cache_db.hpp"

#include <bpt/crs/info/package.hpp>
#include <bpt/bpt.test.hpp>
#include <bpt/error/handle.hpp>
#include <bpt/error/try_catch.hpp>
#include <bpt/util/http/error.hpp>
#include <bpt/util/http/response.hpp>

#include <boost/leaf.hpp>
#include <catch2/catch.hpp>

using namespace neo::sqlite3::literals;

TEST_CASE("Open a cache") {
    auto db  = REQUIRES_LEAF_NOFAIL(bpt::unique_database::open(":memory:").value());
    auto ldr = REQUIRES_LEAF_NOFAIL(bpt::crs::cache_db::open(db));
    (void)ldr;
}

struct empty_loader {
    bpt::unique_database db    = bpt::unique_database::open(":memory:").value();
    bpt::crs::cache_db   cache = bpt::crs::cache_db::open(db);
};

TEST_CASE_METHOD(empty_loader, "Get a non-existent remote") {
    auto url   = neo::url::parse("http://example.com");
    auto entry = REQUIRES_LEAF_NOFAIL(cache.get_remote(url));
    CHECK_FALSE(entry.has_value());
}

TEST_CASE_METHOD(empty_loader, "Sync an invalid repo") {
    auto url = neo::url::parse("http://example.com");
    bpt_leaf_try {
        cache.sync_remote(url);
        FAIL_CHECK("Expected an error to occur");
    }
    bpt_leaf_catch(const bpt::http_error&  exc,
                   bpt::http_response_info resp,
                   bpt::matchv<bpt::e_http_status{404}>) {
        CHECK(exc.status_code() == 404);
        CHECK(resp.status == 404);
    }
    bpt_leaf_catch_all { FAIL_CHECK("Unhandled error: " << diagnostic_info); };
}

TEST_CASE_METHOD(empty_loader, "Enable an invalid repo") {
    auto url = neo::url::parse("http://example.com");
    bpt_leaf_try {
        cache.enable_remote(url);
        FAIL_CHECK("Expected an error to occur");
    }
    bpt_leaf_catch(const bpt::crs::e_no_such_remote_url e) {
        CHECK(e.value == "http://example.com/");
    }
    bpt_leaf_catch_all { FAIL_CHECK("Unhandled error: " << diagnostic_info); };
}
