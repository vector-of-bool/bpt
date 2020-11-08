#include <dds/catalog/remote/http.hpp>

#include <dds/error/errors.hpp>
#include <dds/source/dist.hpp>
#include <dds/util/log.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Convert URL to an HTTP remote listing") {
    auto remote = dds::http_remote_listing::from_url(
        "http://localhost:8000/neo-buffer-0.4.2.tar.gz?dds_strpcmp=1");
}
