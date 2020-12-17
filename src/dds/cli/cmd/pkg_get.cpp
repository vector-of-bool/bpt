#include "../options.hpp"

#include <dds/catalog/get.hpp>
#include <dds/dym.hpp>
#include <dds/error/errors.hpp>
#include <dds/http/session.hpp>
#include <dds/pkg/db.hpp>
#include <dds/util/result.hpp>

#include <boost/leaf/handle_exception.hpp>
#include <json5/parse_data.hpp>

namespace dds::cli::cmd {

static int _pkg_get(const options& opts) {
    auto cat = opts.open_catalog();
    for (const auto& item : opts.pkg.get.pkgs) {
        auto            id = package_id::parse(item);
        dds::dym_target dym;
        auto            info = cat.get(id);
        if (!info) {
            dds::throw_user_error<dds::errc::no_such_catalog_package>(
                "No package in the database matched the ID '{}'.{}", item, dym.sentence_suffix());
        }
        auto tsd  = get_package_sdist(*info);
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
        [&](const json5::parse_error& e, dds::e_http_url bad_url) {
            dds_log(error,
                    "Error parsing JSON5 document package downloaded from [{}]: {}",
                    bad_url.value,
                    e.what());
            return 1;
        },
        [](dds::e_sqlite3_error_exc e) {
            dds_log(error, "Error accessing the package database: {}", e.message);
            return 1;
        },
        [&](dds::e_system_error_exc e, dds::e_http_connect conn) {
            dds_log(error,
                    "Error opening connection to [{}:{}]: {}",
                    conn.host,
                    conn.port,
                    e.message);
            return 1;
        });
}

}  // namespace dds::cli::cmd
