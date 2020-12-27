#include "./dds_http.hpp"

#include <catch2/catch.hpp>

TEST_CASE("Parse a URL") {
    auto pkg = dds::dds_http_remote_pkg::from_url(
        neo::url::parse("dds+http://foo.bar/repo-dir/egg@1.2.3"));
    CHECK(pkg.repo_url.to_string() == "http://foo.bar/repo-dir");
    CHECK(pkg.pkg_id.name == "egg");
    CHECK(pkg.pkg_id.version.to_string() == "1.2.3");
    CHECK(pkg.to_url_string() == "dds+http://foo.bar/repo-dir/egg@1.2.3");
}
