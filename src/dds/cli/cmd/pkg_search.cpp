#include "../options.hpp"

#include "./build_common.hpp"
#include <dds/crs/cache.hpp>
#include <dds/crs/cache_db.hpp>
#include <dds/dym.hpp>
#include <dds/error/marker.hpp>
#include <dds/error/nonesuch.hpp>
#include <dds/util/string.hpp>

#include <boost/leaf.hpp>
#include <fansi/styled.hpp>
#include <fmt/format.h>
#include <neo/ranges.hpp>
#include <neo/sqlite3/connection_ref.hpp>
#include <neo/sqlite3/iter_tuples.hpp>

#include <ranges>

using namespace fansi::literals;
namespace nsql = neo::sqlite3;

namespace {

struct pkg_group_search_result {
    std::string                  name;
    std::vector<semver::version> versions;
    std::string                  remote_url;
};

struct pkg_search_results {
    std::vector<pkg_group_search_result> found;
};

pkg_search_results search_impl(nsql::connection_ref db, std::optional<std::string_view> pattern) {
    auto search_st = *db.prepare(R"(
        SELECT pkg.name,
               group_concat(version, ';;'),
               remote.url
          FROM dds_crs_packages AS pkg
          JOIN dds_crs_remotes AS remote USING(remote_id)
         WHERE lower(pkg.name) GLOB lower(:pattern)
               AND remote_id IN (SELECT remote_id FROM dds_crs_enabled_remotes)
      GROUP BY pkg.name, remote_id
      ORDER BY remote.unique_name, pkg.name
    )");
    // If no pattern, grab _everything_
    auto final_pattern = pattern.value_or("*");
    dds_log(debug, "Searching for packages matching pattern '{}'", final_pattern);
    search_st.bindings()[1] = final_pattern;
    auto rows               = nsql::iter_tuples<std::string, std::string, std::string>(search_st);

    std::vector<pkg_group_search_result> found;
    for (auto [name, versions, remote_url] : rows) {
        dds_log(debug, "Found: {} with versions {} [{}]", name, versions, remote_url);
        auto version_strs = dds::split(versions, ";;");
        auto versions_semver
            = version_strs | std::views::transform(&semver::version::parse) | neo::to_vector;
        std::ranges::sort(versions_semver);
        found.push_back(pkg_group_search_result{
            .name       = name,
            .versions   = versions_semver,
            .remote_url = remote_url,
        });
    }

    if (found.empty()) {
        BOOST_LEAF_THROW_EXCEPTION([&] {
            auto names_st  = *db.prepare(R"(
                SELECT DISTINCT name from dds_crs_packages
                 WHERE remote_id IN (SELECT remote_id FROM dds_crs_enabled_remotes)
                )");
            auto tups      = nsql::iter_tuples<std::string>(names_st);
            auto names_vec = tups
                | std::views::transform([](auto&& row) { return row.template get<0>(); })
                | neo::to_vector;
            auto nearest = dds::did_you_mean(final_pattern, names_vec);
            return dds::e_nonesuch{final_pattern, nearest};
        });
    }

    return pkg_search_results{.found = std::move(found)};
}
}  // namespace

namespace dds::cli::cmd {

static int _pkg_search(const options& opts) {
    auto cache = open_ready_cache(opts);

    auto results = search_impl(cache.db().sqlite3_db(), opts.pkg.search.pattern);
    for (pkg_group_search_result const& found : results.found) {
        fmt::print(
            "    Name: .bold[{}]\n"
            "Versions: .bold[{}]\n"
            "    From: .bold[{}]\n\n"_styled,
            found.name,
            joinstr(", ", found.versions | std::views::transform(&semver::version::to_string)),
            found.remote_url);
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
