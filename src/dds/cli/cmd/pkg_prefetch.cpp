#include "../options.hpp"

#include <dds/crs/cache_db.hpp>
#include <dds/crs/repo.hpp>
#include <dds/util/db/db.hpp>
#include <dds/util/url.hpp>
#include <dds/error/result.hpp>

namespace dds::cli::cmd {

int pkg_prefetch(const options& opts) {
    auto db    = dds::unique_database::open("crs-cache.db").value();
    auto cache = dds::crs::cache_db::open(db);
    for (auto& r : opts.use_repos) {
        auto url = dds::guess_url_from_string(r);
        cache.sync_repo(url);
    }
    return 0;
}

}  // namespace dds::cli::cmd
