#include "./listing.hpp"

#include <catch2/catch.hpp>

TEST_CASE("Round trip a URL") {
    auto listing
        = dds::any_remote_pkg::from_url(neo::url::parse("http://example.org/package.tar.gz"));
    CHECK(listing.to_url_string() == "http://example.org/package.tar.gz");

    listing = dds::any_remote_pkg::from_url(neo::url::parse("git://example.org/repo#wat"));
    CHECK(listing.to_url_string() == "git://example.org/repo#wat");
}
