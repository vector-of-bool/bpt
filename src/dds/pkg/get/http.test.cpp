#include "./http.hpp"

#include <dds/error/errors.hpp>
#include <dds/sdist/dist.hpp>
#include <dds/util/log.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Convert from URL") {
    auto listing = dds::http_remote_pkg::from_url(neo::url::parse("http://example.org/foo"));
    CHECK(listing.to_url_string() == "http://example.org/foo");
    listing.strip_n_components = 4;
    CHECK(listing.to_url_string() == "http://example.org/foo?__dds_strpcmp=4");

    listing = dds::http_remote_pkg::from_url(
        neo::url::parse("http://example.org/foo?bar=baz;__dds_strpcmp=7;thing=foo#fragment"));
    CHECK(listing.strip_n_components == 7);
    CHECK(listing.to_url_string() == "http://example.org/foo?bar=baz;thing=foo;__dds_strpcmp=7");
}
