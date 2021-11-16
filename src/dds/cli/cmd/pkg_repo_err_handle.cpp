#include "./pkg_repo_err_handle.hpp"

#include "../options.hpp"

#include <dds/dym.hpp>
#include <dds/error/errors.hpp>
#include <dds/error/nonesuch.hpp>
#include <dds/pkg/remote.hpp>
#include <dds/util/http/pool.hpp>
#include <dds/util/log.hpp>
#include <dds/util/result.hpp>

#include <boost/leaf/handle_exception.hpp>
#include <fansi/styled.hpp>
#include <json5/parse_data.hpp>
#include <neo/sqlite3/error.hpp>
#include <neo/url.hpp>

using namespace fansi::literals;

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
        [](dds::http_status_error, matchv<dds::e_http_status{404}>, neo::url bad_url) {
            dds_log(error,
                    "Failed to download non-existent resource [.br.red[{}]] from the remote "
                    "server. Is this a valid dds repository?"_styled,
                    bad_url.to_string());
            return 1;
        },
        [](dds::http_status_error, dds::http_response_info resp, neo::url bad_url) {
            dds_log(
                error,
                "An HTTP error occurred while requesting [.br.red[{}]]: HTTP Status .red[{} {}]"_styled,
                bad_url.to_string(),
                resp.status,
                resp.status_message);
            return 1;
        },
        [](const json5::parse_error& e, neo::url bad_url) {
            dds_log(error,
                    "Error parsing JSON downloaded from URL [.br.red[{}]]: {}"_styled,
                    bad_url.to_string(),
                    e.what());
            return 1;
        },
        [](boost::leaf::catch_<neo::sqlite3::error> e, neo::url url) {
            dds_log(error,
                    "Error accessing remote database [.br.red[{}]]: {}"_styled,
                    url.to_string(),
                    e.value().what());
            return 1;
        },
        [](boost::leaf::catch_<neo::sqlite3::error> e) {
            dds_log(error, "Unexpected database error: {}", e.value().what());
            return 1;
        },
        [](dds::e_system_error_exc e, dds::network_origin conn) {
            dds_log(error,
                    "Error communicating with [.br.red[{}://{}:{}]]: {}"_styled,
                    conn.protocol,
                    conn.hostname,
                    conn.port,
                    e.message);
            return 1;
        },
        [](matchv<pkg_repo_subcommand::remove>, e_nonesuch missing) {
            missing.log_error(
                "Cannot delete remote '.br.red[{}]', as no such remote repository is locally registered by that name."_styled);
            write_error_marker("repo-rm-no-such-repo");
            return 1;
        });
}
