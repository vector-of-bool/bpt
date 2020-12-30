#include "./error_handler.hpp"
#include "./options.hpp"

#include <dds/error/errors.hpp>
#include <dds/error/toolchain.hpp>
#include <dds/util/http/pool.hpp>
#include <dds/util/log.hpp>
#include <dds/util/result.hpp>
#include <dds/util/signal.hpp>

#include <boost/leaf/common.hpp>
#include <boost/leaf/handle_error.hpp>
#include <boost/leaf/handle_exception.hpp>
#include <boost/leaf/result.hpp>
#include <fansi/styled.hpp>
#include <fmt/ostream.h>
#include <json5/parse_data.hpp>
#include <neo/scope.hpp>
#include <neo/url/parse.hpp>

#include <fstream>

using namespace dds;
using namespace fansi::literals;

namespace {

auto handlers = std::tuple(  //
    [](neo::url_validation_error exc, e_url_string bad_url) {
        dds_log(error, "Invalid URL '{}': {}", bad_url.value, exc.what());
        return 1;
    },
    [](boost::leaf::catch_<error_base> exc,
       json5::parse_error              parse_err,
       boost::leaf::e_file_name*       maybe_fpath) {
        dds_log(error, "{}", exc.value().what());
        dds_log(error, "Invalid JSON5 was found: {}", parse_err.what());
        if (maybe_fpath) {
            dds_log(error, "  (While reading from [{}])", maybe_fpath->value);
        }
        dds_log(error, "{}", exc.value().explanation());
        write_error_marker("package-json5-parse-error");
        return 1;
    },
    [](user_error<errc::test_failure> exc, matchv<cli::subcommand::build>) {
        write_error_marker("build-failed-test-failed");
        dds_log(error, "{}", exc.what());
        dds_log(error, "{}", exc.explanation());
        dds_log(error, "Refer: {}", exc.error_reference());
        return 1;
    },
    [](boost::leaf::catch_<error_base> exc) {
        dds_log(error, "{}", exc.value().what());
        dds_log(error, "{}", exc.value().explanation());
        dds_log(error, "Refer: {}", exc.value().error_reference());
        return 1;
    },
    [](user_cancelled) {
        dds_log(critical, "Operation cancelled by the user");
        return 2;
    },
    [](e_system_error_exc e, neo::url url, http_response_info) {
        dds_log(error,
                "An error occured while downloading [.bold.red[{}]]: {}"_styled,
                url.to_string(),
                e.message);
        return 1;
    },
    [](e_system_error_exc e, network_origin origin, neo::url* url) {
        dds_log(error,
                "Network error communicating with .bold.red[{}://{}:{}]: {}"_styled,
                origin.protocol,
                origin.hostname,
                origin.port,
                e.message);
        if (url) {
            dds_log(error, "  (While accessing URL [.bold.red[{}]])"_styled, url->to_string());
        }
        return 1;
    },
    [](e_system_error_exc err,
       e_loading_toolchain,
       e_toolchain_file* tc_file,
       e_toolchain_builtin*) {
        dds_log(error, "Failed to load toolchain: .br.yellow[{}]"_styled, err.message);
        if (tc_file) {
            dds_log(error, "  (While loading from file [.bold.red[{}]])"_styled, tc_file->value);
        }
        return 1;
    },
    [](e_system_error_exc exc, boost::leaf::verbose_diagnostic_info const& diag) {
        dds_log(critical,
                "An unhandled std::system_error arose. THIS IS A DDS BUG! Info: {}",
                diag);
        dds_log(critical, "Exception message from std::system_error: {}", exc.message);
        return 42;
    },
    [](boost::leaf::verbose_diagnostic_info const& diag) {
        dds_log(critical, "An unhandled error arose. THIS IS A DDS BUG! Info: {}", diag);
        return 42;
    });
}  // namespace

int dds::handle_cli_errors(std::function<int()> fn) noexcept {
    return boost::leaf::try_catch(
        [&] {
            try {
                return fn();
            } catch (...) {
                capture_exception();
            }
        },
        handlers);
}
