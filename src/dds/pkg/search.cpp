#include "./search.hpp"

#include <dds/dym.hpp>
#include <dds/error/nonesuch.hpp>
#include <dds/error/result.hpp>
#include <dds/util/log.hpp>
#include <dds/util/string.hpp>

#include <neo/fwd.hpp>
#include <neo/ranges.hpp>
#include <neo/sqlite3/database.hpp>
#include <neo/sqlite3/iter_tuples.hpp>
#include <neo/tl.hpp>

using namespace dds;
namespace nsql = neo::sqlite3;

result<pkg_search_results> dds::pkg_search(nsql::database_ref              db,
                                           std::optional<std::string_view> pattern) noexcept {
    auto search_st = db.prepare(R"(
        SELECT pkg.name,
               group_concat(version, ';;'),
               description,
               remote.name,
               remote.url
          FROM dds_pkgs AS pkg
          JOIN dds_pkg_remotes AS remote USING(remote_id)
         WHERE lower(pkg.name) GLOB lower(:pattern)
      GROUP BY pkg.name, remote_id, description
      ORDER BY remote.name, pkg.name
    )");
    // If no pattern, grab _everything_
    auto final_pattern = pattern.value_or("*");
    dds_log(debug, "Searching for packages matching pattern '{}'", final_pattern);
    search_st.bindings()[1] = final_pattern;
    auto rows = nsql::iter_tuples<std::string, std::string, std::string, std::string, std::string>(
        search_st);

    std::vector<pkg_group_search_result> found;
    for (auto [name, versions_, desc, remote_name, remote_url] : rows) {
        dds_log(debug,
                "Found: {} with versions {} (Description: {}) from {} [{}]",
                name,
                versions_,
                desc,
                remote_name,
                remote_url);
        auto versions =                                                  //
            split(versions_, ";;") | neo::lref                           //
            | std::views::transform(NEO_TL(semver::version::parse(_1)))  //
            | neo::to_vector;
        std::ranges::sort(versions);
        found.push_back(pkg_group_search_result{
            .name        = name,
            .versions    = versions,
            .description = desc,
            .remote_name = remote_name,
        });
    }

    if (found.empty()) {
        return boost::leaf::new_error([&] {
            auto names_st  = db.prepare("SELECT DISTINCT name FROM dds_pkgs");
            auto names_vec =                                                   //
                nsql::iter_tuples<std::string>(names_st)                       //
                | neo::lref                                                    //
                | std::views::transform(NEO_TL(std::string(std::get<0>(_1))))  //
                | neo::to_vector;
            auto nearest = dds::did_you_mean(final_pattern, names_vec);
            return e_nonesuch{final_pattern, nearest};
        });
    }

    return pkg_search_results{.found = std::move(found)};
}
