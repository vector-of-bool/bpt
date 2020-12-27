#include "./github.hpp"

#include <catch2/catch.hpp>

TEST_CASE("Parse a github: URL") {
    auto gh_pkg
        = dds::github_remote_pkg::from_url(neo::url::parse("github:vector-of-bool/neo-fun/0.6.0"));
    CHECK(gh_pkg.owner == "vector-of-bool");
    CHECK(gh_pkg.reponame == "neo-fun");
    CHECK(gh_pkg.ref == "0.6.0");
}
