#include "./error_handler.hpp"
#include "./options.hpp"

#include <dds/error/errors.hpp>
#include <dds/util/log.hpp>
#include <dds/util/result.hpp>
#include <dds/util/signal.hpp>

#include <boost/leaf/common.hpp>
#include <boost/leaf/handle_error.hpp>
#include <boost/leaf/handle_exception.hpp>
#include <boost/leaf/result.hpp>
#include <fmt/ostream.h>
#include <json5/parse_data.hpp>
#include <neo/scope.hpp>
#include <neo/url/parse.hpp>

#include <fstream>

namespace {

template <dds::cli::subcommand Val>
using subcommand = boost::leaf::match<dds::cli::subcommand, Val>;

auto handlers = std::tuple(  //
    [](neo::url_validation_error exc, dds::e_url_string bad_url) {
        dds_log(error, "Invalid URL '{}': {}", bad_url.value, exc.what());
        return 1;
    },
    [](boost::leaf::catch_<dds::error_base> exc,
       json5::parse_error                   parse_err,
       boost::leaf::e_file_name*            maybe_fpath) {
        dds_log(error, "{}", exc.value().what());
        dds_log(error, "Invalid JSON5 was found: {}", parse_err.what());
        if (maybe_fpath) {
            dds_log(error, "  (While reading from [{}])", maybe_fpath->value);
        }
        dds_log(error, "{}", exc.value().explanation());
        return 1;
    },
    [](boost::leaf::catch_<dds::error_base> exc) {
        dds_log(error, "{}", exc.value().what());
        dds_log(error, "{}", exc.value().explanation());
        dds_log(error, "Refer: {}", exc.value().error_reference());
        return 1;
    },
    [](dds::user_cancelled) {
        dds_log(critical, "Operation cancelled by the user");
        return 2;
    },
    [](boost::leaf::verbose_diagnostic_info const& diag) {
        dds_log(critical, "An unhandled error arose. THIS IS A DDS BUG! Info: {}", diag);
        return 42;
    });
}  // namespace

int dds::handle_cli_errors(std::function<int()> fn) noexcept {
    return boost::leaf::try_catch(
        [&] {
            boost::leaf::context<dds::e_error_marker> marker_ctx;
            marker_ctx.activate();
            neo_defer {
                marker_ctx.deactivate();
                marker_ctx.handle_error<void>(
                    boost::leaf::current_error(),
                    [](dds::e_error_marker mark) {
                        dds_log(trace, "[error marker {}]", mark.value);
                        auto efile_path = std::getenv("DDS_WRITE_ERROR_MARKER");
                        if (efile_path) {
                            std::ofstream outfile{efile_path, std::ios::binary};
                            fmt::print(outfile, "{}", mark.value);
                        }
                    },
                    [] {});
            };
            return fn();
        },
        handlers);
}
