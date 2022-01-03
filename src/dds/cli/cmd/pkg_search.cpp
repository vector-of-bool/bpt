#include "../options.hpp"

#include "./build_common.hpp"
#include <dds/crs/cache.hpp>
#include <dds/crs/cache_db.hpp>
#include <dds/error/nonesuch.hpp>
#include <dds/pkg/search.hpp>
#include <dds/util/result.hpp>
#include <dds/util/string.hpp>

#include <boost/leaf.hpp>
#include <fansi/styled.hpp>
#include <fmt/format.h>
#include <range/v3/view/transform.hpp>

using namespace fansi::literals;

namespace dds::cli::cmd {

static int _pkg_search(const options& opts) {
    auto cache = open_ready_cache(opts);

    auto results
        = dds::pkg_search(cache.metadata_db().sqlite3_db(), opts.pkg.search.pattern).value();
    for (pkg_group_search_result const& found : results.found) {
        fmt::print(
            "    Name: .bold[{}]\n"
            "Versions: .bold[{}]\n"
            "    From: .bold[{}]\n\n"_styled,
            found.name,
            joinstr(", ", found.versions | ranges::views::transform(&semver::version::to_string)),
            found.remote_url);
    }

    if (results.found.empty()) {
        dds_log(error,
                "There are no packages that match the given pattern \".bold.red[{}]\""_styled,
                opts.pkg.search.pattern.value_or("*"));
        write_error_marker("pkg-search-no-result");
        return 1;
    }
    return 0;
}

int pkg_search(const options& opts) {
    return boost::leaf::try_catch(
        [&] { return _pkg_search(opts); },
        [](e_nonesuch missing) {
            missing.log_error(
                "There are no packages that match the given pattern \".bold.red[{}]\""_styled);
            write_error_marker("pkg-search-no-result");
            return 1;
        });
}

}  // namespace dds::cli::cmd
