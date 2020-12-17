#include "./pkg_repo_err_handle.hpp"

#include <dds/http/session.hpp>
#include <dds/util/log.hpp>
#include <dds/util/result.hpp>

#include <boost/leaf/handle_exception.hpp>
#include <json5/parse_data.hpp>
#include <neo/url/parse.hpp>

int dds::cli::cmd::handle_pkg_repo_remote_errors(std::function<int()> fn) {
    return boost::leaf::try_catch(
        [&] {
            try {
                return fn();
            } catch (...) {
                dds::capture_exception();
            }
        },
        [&](neo::url_validation_error url_err, dds::e_url_string bad_url) {
            dds_log(error, "Invalid URL [{}]: {}", bad_url.value, url_err.what());
            return 1;
        },
        [&](const json5::parse_error& e, dds::e_http_url bad_url) {
            dds_log(error,
                    "Error parsing JSON downloaded from URL [{}]: {}",
                    bad_url.value,
                    e.what());
            return 1;
        },
        [](dds::e_sqlite3_error_exc e, dds::e_url_string url) {
            dds_log(error, "Error accessing remote database (From {}): {}", url.value, e.message);
            return 1;
        },
        [](dds::e_sqlite3_error_exc e) {
            dds_log(error, "Unexpected database error: {}", e.message);
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
