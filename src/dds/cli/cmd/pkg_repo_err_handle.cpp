#include "./pkg_repo_err_handle.hpp"

#include <dds/util/http/pool.hpp>
#include <dds/util/log.hpp>
#include <dds/util/result.hpp>

#include <boost/leaf/handle_exception.hpp>
#include <json5/parse_data.hpp>
#include <neo/url.hpp>

int dds::cli::cmd::handle_pkg_repo_remote_errors(std::function<int()> fn) {
    return boost::leaf::try_catch(
        [&] {
            try {
                return fn();
            } catch (...) {
                dds::capture_exception();
            }
        },
        [](neo::url_validation_error url_err, neo::url bad_url) {
            dds_log(error, "Invalid URL [{}]: {}", bad_url.to_string(), url_err.what());
            return 1;
        },
        [](dds::http_status_error err, dds::http_response_info resp, neo::url bad_url) {
            dds_log(error,
                    "An HTTP error occured while requesting [{}]: HTTP Status {} {}",
                    err.what(),
                    bad_url.to_string(),
                    resp.status,
                    resp.status_message);
            return 1;
        },
        [](const json5::parse_error& e, neo::url bad_url) {
            dds_log(error,
                    "Error parsing JSON downloaded from URL [{}]: {}",
                    bad_url.to_string(),
                    e.what());
            return 1;
        },
        [](dds::e_sqlite3_error_exc e, neo::url url) {
            dds_log(error, "Error accessing remote database [{}]: {}", url.to_string(), e.message);
            return 1;
        },
        [](dds::e_sqlite3_error_exc e) {
            dds_log(error, "Unexpected database error: {}", e.message);
            return 1;
        },
        [](dds::e_system_error_exc e, dds::network_origin conn) {
            dds_log(error,
                    "Error communicating with [{}://{}:{}]: {}",
                    conn.protocol,
                    conn.hostname,
                    conn.port,
                    e.message);
            return 1;
        });
}
