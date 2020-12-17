#include "./error_handler.hpp"
#include "./options.hpp"

#include <dds/error/errors.hpp>
#include <dds/util/log.hpp>
#include <dds/util/result.hpp>
#include <dds/util/signal.hpp>

#include <boost/leaf/handle_error.hpp>
#include <boost/leaf/handle_exception.hpp>
#include <boost/leaf/result.hpp>
#include <fmt/ostream.h>
#include <neo/url/parse.hpp>

namespace {

template <dds::cli::subcommand Val>
using subcommand = boost::leaf::match<dds::cli::subcommand, Val>;

auto handlers = std::tuple(  //
    [](neo::url_validation_error exc, dds::e_url_string bad_url) {
        dds_log(error, "Invalid URL '{}': {}", bad_url.value, exc.what());
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
    return boost::leaf::try_handle_all([&]() -> boost::leaf::result<int> { return fn(); },
                                       handlers);
}
