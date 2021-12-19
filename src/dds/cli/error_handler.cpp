#include "./error_handler.hpp"
#include "./options.hpp"

#include <dds/crs/error.hpp>
#include <dds/error/errors.hpp>
#include <dds/error/toolchain.hpp>
#include <dds/sdist/library/manifest.hpp>
#include <dds/sdist/package.hpp>
#include <dds/util/compress.hpp>
#include <dds/util/fs/io.hpp>
#include <dds/util/http/error.hpp>
#include <dds/util/http/pool.hpp>
#include <dds/util/log.hpp>
#include <dds/util/result.hpp>
#include <dds/util/signal.hpp>

#include <boost/leaf/common.hpp>
#include <boost/leaf/pred.hpp>
#include <boost/leaf/result.hpp>
#include <fansi/styled.hpp>
#include <fmt/ostream.h>
#include <json5/parse_data.hpp>
#include <neo/scope.hpp>
#include <neo/sqlite3/error.hpp>
#include <neo/url/parse.hpp>

#include <fstream>

using namespace dds;
using namespace fansi::literals;

using boost::leaf::catch_;

namespace {

auto handlers = std::tuple(  //
    [](neo::url_validation_error exc, e_url_string bad_url) {
        dds_log(error, "Invalid URL '{}': {}", bad_url.value, exc.what());
        return 1;
    },
    [](boost::leaf::catch_<error_base> exc,
       json5::parse_error              parse_err,
       boost::leaf::e_file_name*       maybe_fpath) {
        dds_log(error, "{}", exc.matched.what());
        dds_log(error, "Invalid JSON5 was found: {}", parse_err.what());
        if (maybe_fpath) {
            dds_log(error, "  (While reading from [{}])", maybe_fpath->value);
        }
        dds_log(error, "{}", exc.matched.explanation());
        write_error_marker("package-json5-parse-error");
        return 1;
    },
    [](boost::leaf::catch_<error_base> exc) {
        dds_log(error, "{}", exc.matched.what());
        dds_log(error, "{}", exc.matched.explanation());
        dds_log(error, "Refer: {}", exc.matched.error_reference());
        return 1;
    },
    [](user_cancelled) {
        dds_log(critical, "Operation cancelled by the user");
        return 2;
    },
    [](const std::system_error& e, neo::url url, http_response_info) {
        dds_log(error,
                "An error occurred while downloading [.bold.red[{}]]: {}"_styled,
                url.to_string(),
                e.code().message());
        return 1;
    },
    [](const std::system_error& e, network_origin origin, neo::url* url) {
        dds_log(error,
                "Network error communicating with .bold.red[{}://{}:{}]: {}"_styled,
                origin.protocol,
                origin.hostname,
                origin.port,
                e.code().message());
        if (url) {
            dds_log(error, "  (While accessing URL [.bold.red[{}]])"_styled, url->to_string());
        }
        return 1;
    },
    [](const std::system_error& err, e_loading_toolchain, e_toolchain_file* tc_file) {
        dds_log(error, "Failed to load toolchain: .br.yellow[{}]"_styled, err.code().message());
        if (tc_file) {
            dds_log(error, "  (While loading from file [.bold.red[{}]])"_styled, tc_file->value);
        }
        write_error_marker("bad-toolchain");
        return 1;
    },
    [](e_name_str               badname,
       invalid_name_reason      why,
       e_dependency_string      depstr,
       e_package_manifest_path* pkman_path) {
        dds_log(
            error,
            "Invalid package name '.bold.red[{}]' in dependency string '.br.red[{}]': .br.yellow[{}]"_styled,
            badname.value,
            depstr.value,
            invalid_name_reason_str(why));
        if (pkman_path) {
            dds_log(error,
                    "  (While reading package manifest from [.bold.yellow[{}]])"_styled,
                    pkman_path->value);
        }
        write_error_marker("invalid-pkg-dep-name");
        return 1;
    },
    [](e_pkg_name_str,
       e_name_str               badname,
       invalid_name_reason      why,
       e_package_manifest_path* pkman_path) {
        dds_log(error,
                "Invalid package name '.bold.red[{}]': .br.yellow[{}]"_styled,
                badname.value,
                invalid_name_reason_str(why));
        if (pkman_path) {
            dds_log(error,
                    "  (While reading package manifest from [.bold.yellow[{}]])"_styled,
                    pkman_path->value);
        }
        write_error_marker("invalid-pkg-name");
        return 1;
    },
    [](e_pkg_namespace_str,
       e_name_str               badname,
       invalid_name_reason      why,
       e_package_manifest_path* pkman_path) {
        dds_log(error,
                "Invalid package namespace '.bold.red[{}]': .br.yellow[{}]"_styled,
                badname.value,
                invalid_name_reason_str(why));
        if (pkman_path) {
            dds_log(error,
                    "  (While reading package manifest from [.bold.yellow[{}]])"_styled,
                    pkman_path->value);
        }
        write_error_marker("invalid-pkg-namespace-name");
        return 1;
    },
    [](e_library_manifest_path libpath, invalid_name_reason why, e_name_str badname) {
        dds_log(error,
                "Invalid library name '.bold.red[{}]': .br.yellow[{}]"_styled,
                badname.value,
                invalid_name_reason_str(why));
        dds_log(error,
                "  (While reading library manifest from [.bold.yellow[{}]])"_styled,
                libpath.value);
        write_error_marker("invalid-lib-name");
        return 1;
    },
    [](e_library_manifest_path libpath, lm::e_invalid_usage_string usage) {
        dds_log(error, "Invalid 'uses' library identifier '.bold.red[{}]'"_styled, usage.value);
        dds_log(error,
                "  (While reading library manifest from [.bold.yellow[{}]])"_styled,
                libpath.value);
        write_error_marker("invalid-uses-spec");
        return 1;
    },
    [](dds::crs::e_sync_remote sync_repo,
       dds::e_decompress_error err,
       dds::e_read_file_path   read_file) {
        dds_log(error,
                "Error while sychronizing package data from .bold.yellow[{}]"_styled,
                sync_repo.value.to_string());
        dds_log(
            error,
            "Error decompressing remote repository database [.br.yellow[{}]]: .bold.red[{}]"_styled,
            read_file.value.string(),
            err.value);
        write_error_marker("repo-sync-invalid-db-gz");
        return 1;
    },
    [](catch_<neo::sqlite3::error> exc, dds::crs::e_sync_remote sync_repo) {
        dds_log(error,
                "SQLite error while importing data from .br.yellow[{}]: .br.red[{}]"_styled,
                sync_repo.value.to_string(),
                exc.matched.what());
        dds_log(error,
                "It's possilbe that the downloaded SQLite database is corrupt, invalid, or "
                "incompatible with this version of dds");
        return 1;
    },
    [](matchv<dds::e_http_status{404}>, dds::crs::e_sync_remote sync_repo, neo::url req_url) {
        dds_log(
            error,
            "Received an .bold.red[HTTP 404 Not Found] error while synchronizing a repository from .bold.yellow[{}]"_styled,
            sync_repo.value.to_string());
        dds_log(error,
                "The given location might not be a valid package repository, or the URL might be "
                "spelled incorrectly.");
        dds_log(error,
                "  (The missing resource URL is [.bold.yellow[{}]])"_styled,
                req_url.to_string());
        write_error_marker("repo-sync-http-404");
        return 1;
    },
    [](catch_<dds::http_error>,
       neo::url                req_url,
       dds::crs::e_sync_remote sync_repo,
       dds::http_response_info resp) {
        dds_log(
            error,
            "HTTP .br.red[{}] (.br.red[{}]) error while trying to synchronize remote package repository [.bold.yellow[{}]]"_styled,
            resp.status,
            resp.status_message,
            sync_repo.value.to_string());
        dds_log(error,
                "  Error requesting [.bold.yellow[{}]]: .bold.red[HTTP {} {}]"_styled,
                req_url.to_string(),
                resp.status,
                resp.status_message);
        write_error_marker("repo-sync-http-error");
        return 1;
    },
    [](catch_<neo::sqlite3::error> exc, boost::leaf::verbose_diagnostic_info const& diag) {
        dds_log(critical,
                "An unhandled SQLite error arose. .bold.red[THIS IS A DDS BUG!] Info: {}"_styled,
                diag);
        dds_log(critical,
                "Exception message from neo::sqlite3::error: .bold.red[{}]"_styled,
                exc.matched.what());
        return 42;
    },
    [](const std::system_error& exc, boost::leaf::verbose_diagnostic_info const& diag) {
        dds_log(
            critical,
            "An unhandled std::system_error arose. .bold.red[THIS IS A DDS BUG!] Info: {}"_styled,
            diag);
        dds_log(critical,
                "Exception message from std::system_error: .bold.red[{}]",
                exc.code().message());
        return 42;
    },
    [](boost::leaf::verbose_diagnostic_info const& diag) {
        dds_log(critical, "An unhandled error arose. .bold.red[THIS IS A DDS BUG!] Info: {}", diag);
        return 42;
    });
}  // namespace

int dds::handle_cli_errors(std::function<int()> fn) noexcept {
    return boost::leaf::try_catch(fn, handlers);
}
