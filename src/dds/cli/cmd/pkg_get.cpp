#include "../options.hpp"

#include <dds/dym.hpp>
#include <dds/error/errors.hpp>
#include <dds/error/nonesuch.hpp>
#include <dds/pkg/db.hpp>
#include <dds/pkg/get/get.hpp>
#include <dds/util/http/pool.hpp>
#include <dds/util/result.hpp>

#include <boost/leaf/handle_exception.hpp>
#include <json5/parse_data.hpp>
#include <neo/url.hpp>

namespace dds::cli::cmd {

static int _pkg_get(const options& opts) {
    auto cat = opts.open_pkg_db();
    for (const auto& item : opts.pkg.get.pkgs) {
        auto id   = pkg_id::parse(item);
        auto info = *cat.get(id);
        auto tsd  = get_package_sdist(info);
        auto dest = opts.out_path.value_or(fs::current_path()) / id.to_string();
        dds_log(info, "Create sdist at {}", dest.string());
        fs::remove_all(dest);
        safe_rename(tsd.sdist.path, dest);
    }
    return 0;
}

int pkg_get(const options& opts) {
    return boost::leaf::try_catch(  //
        [&] {
            try {
                return _pkg_get(opts);
            } catch (...) {
                dds::capture_exception();
            }
        },
        [&](neo::url_validation_error url_err, dds::e_url_string bad_url) {
            dds_log(error,
                    "Invalid package URL in the database [{}]: {}",
                    bad_url.value,
                    url_err.what());
            return 1;
        },
        [&](const json5::parse_error& e, neo::url bad_url) {
            dds_log(error,
                    "Error parsing JSON5 document package downloaded from [{}]: {}",
                    bad_url.to_string(),
                    e.what());
            return 1;
        },
        [](boost::leaf::catch_<neo::sqlite3::error> e) {
            dds_log(error, "Error accessing the package database: {}", e.value().what());
            return 1;
        },
        [](e_nonesuch nonesuch) -> int {
            nonesuch.log_error("There is no entry in the package database for '{}'.");
            write_error_marker("pkg-get-no-pkg-id-listing");
            return 1;
        },
        [&](dds::e_system_error_exc e, dds::network_origin conn) {
            dds_log(error,
                    "Error opening connection to [{}:{}]: {}",
                    conn.hostname,
                    conn.port,
                    e.message);
            return 1;
        });
}

}  // namespace dds::cli::cmd
