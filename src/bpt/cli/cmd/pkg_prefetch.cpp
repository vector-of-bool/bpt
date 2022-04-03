#include "../options.hpp"

#include "./cache_util.hpp"

#include <bpt/crs/cache.hpp>
#include <bpt/crs/cache_db.hpp>
#include <bpt/crs/info/pkg_id.hpp>
#include <bpt/crs/repo.hpp>
#include <bpt/util/url.hpp>

namespace dds::cli::cmd {

int pkg_prefetch(const options& opts) {
    auto cache = open_ready_cache(opts);
    for (auto& pkg_str : opts.pkg.prefetch.pkgs) {
        auto pid = crs::pkg_id::parse(pkg_str);
        cache.prefetch(pid);
    }
    return 0;
}

}  // namespace dds::cli::cmd
