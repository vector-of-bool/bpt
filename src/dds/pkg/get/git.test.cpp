#include "./git.hpp"

#include <catch2/catch.hpp>

TEST_CASE("Round-trip a URL") {
    auto git = dds::git_remote_pkg::from_url(
        neo::url::parse("http://github.com/vector-of-bool/neo-fun.git#0.4.0"));
    CHECK(git.to_url_string() == "git+http://github.com/vector-of-bool/neo-fun.git#0.4.0");
}
