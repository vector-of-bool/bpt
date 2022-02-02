#include "./error_handler.hpp"
#include "./options.hpp"

#include <dds/crs/error.hpp>
#include <dds/deps.hpp>
#include <dds/error/errors.hpp>
#include <dds/error/toolchain.hpp>
#include <dds/project/dependency.hpp>
#include <dds/project/error.hpp>
#include <dds/project/spdx.hpp>
#include <dds/sdist/error.hpp>
#include <dds/usage_reqs.hpp>
#include <dds/util/compress.hpp>
#include <dds/util/fs/io.hpp>
#include <dds/util/http/error.hpp>
#include <dds/util/http/pool.hpp>
#include <dds/util/json5/error.hpp>
#include <dds/util/log.hpp>
#include <dds/util/result.hpp>
#include <dds/util/signal.hpp>
#include <dds/util/url.hpp>
#include <dds/util/yaml/errors.hpp>

#include <boost/leaf/common.hpp>
#include <boost/leaf/pred.hpp>
#include <boost/leaf/result.hpp>
#include <fansi/styled.hpp>
#include <fmt/ostream.h>
#include <json5/parse_data.hpp>
#include <neo/scope.hpp>
#include <neo/sqlite3/error.hpp>
#include <neo/url/parse.hpp>
#include <semester/walk.hpp>

#include <fstream>

using namespace dds;
using namespace fansi::literals;

using boost::leaf::catch_;

namespace {

auto handlers = std::tuple(  //
    [](e_sdist_from_directory       sdist_dirpath,
       e_json_parse_error           error,
       const std::filesystem::path* maybe_fpath) {
        dds_log(
            error,
            "Invalid metadata file while opening source distribution/project in [.bold.yellow[{}]]"_styled,
            sdist_dirpath.value.string());
        dds_log(error, "Invalid JSON file: .bold.red[{}]"_styled, error.value);
        if (maybe_fpath) {
            dds_log(error, "  (While reading from [{}])", maybe_fpath->string());
        }
        write_error_marker("package-json5-parse-error");
        return 1;
    },
    [](e_sdist_from_directory       sdist_dirpath,
       sbs::e_yaml_parse_error      error,
       const std::filesystem::path* maybe_fpath) {
        dds_log(
            error,
            "Invalid metadata file while opening source distribution/project in [.bold.yellow[{}]]"_styled,
            sdist_dirpath.value.string());
        dds_log(error, "Invalid YAML file: .bold.red[{}]"_styled, error.value);
        if (maybe_fpath) {
            dds_log(error, "  (While reading from [{}])", maybe_fpath->string());
        }
        write_error_marker("package-yaml-parse-error");
        return 1;
    },
    [](e_sdist_from_directory, e_parse_project_manifest_path pkg_yaml, e_bad_pkg_yaml_key badkey) {
        dds_log(error,
                "Error loading project info from [.bold.yellow[{}]]"_styled,
                pkg_yaml.value.string());
        dds_log(error, "Unknown project property '.bold.red[{}]'"_styled, badkey.given);
        if (badkey.nearest.has_value()) {
            dds_log(error, "  (Did you mean '.bold.green[{}]'?)"_styled, *badkey.nearest);
        }
        return 1;
    },
    [](const semester::walk_error& exc,
       e_sdist_from_directory,
       e_parse_project_manifest_path pkg_yaml) {
        dds_log(error,
                "Error loading project info from [.bold.yellow[{}]]: .bold.red[{}]"_styled,
                pkg_yaml.value.string(),
                exc.what());
        return 1;
    },
    [](const neo::url_validation_error& exc,
       e_sdist_from_directory,
       e_parse_project_manifest_path pkg_yaml,
       e_url_string                  str) {
        dds_log(
            error,
            "Error while parsing URL string '.bold.yellow[{}]' in [.bold.yellow[{}]]: .bold.red[{}]"_styled,
            str.value,
            pkg_yaml.value.string(),
            exc.what());
        return 1;
    },
    [](sbs::e_bad_spdx_expression err,
       sbs::e_spdx_license_str    spdx_str,
       e_sdist_from_directory,
       e_parse_project_manifest_path pkg_yaml) {
        dds_log(error,
                "Invalid SPDX license expression '.bold.yellow[{}]': .bold.red[{}]"_styled,
                spdx_str.value,
                err.value);
        dds_log(error,
                "  (While reading project manifest from [.bold.yellow[{}]]"_styled,
                pkg_yaml.value.string());
        write_error_marker("invalid-spdx");
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
    [](e_name_str                           badname,
       invalid_name_reason                  why,
       e_parse_dep_range_shorthand_string   depstr,
       e_parse_project_manifest_path const* prman_path) {
        dds_log(
            error,
            "Invalid package name '.bold.red[{}]' in dependency string '.br.red[{}]': .br.yellow[{}]"_styled,
            badname.value,
            depstr.value,
            invalid_name_reason_str(why));
        if (prman_path) {
            dds_log(error,
                    "  (While reading project manifest from [.bold.yellow[{}]])"_styled,
                    prman_path->value);
        }
        write_error_marker("invalid-pkg-dep-name");
        return 1;
    },
    [](e_name_str badname, invalid_name_reason why, e_parse_project_manifest_path* prman_path) {
        dds_log(error,
                "Invalid name string '.bold.red[{}]': .br.yellow[{}]"_styled,
                badname.value,
                invalid_name_reason_str(why));
        if (prman_path) {
            dds_log(error,
                    "  (While reading project manifest from [.bold.yellow[{}]])"_styled,
                    prman_path->value);
        }
        write_error_marker("invalid-name");
        return 1;
    },
    [](e_parse_dep_shorthand_string              given,
       e_parse_dep_range_shorthand_string const* range_part,
       e_human_message                           msg,
       e_parse_project_manifest_path const*      proj_man) {
        dds_log(error,
                "Invalid dependency shorthand string '.bold.yellow[{}]'"_styled,
                given.value);
        if (range_part) {
            dds_log(error,
                    "  (While parsing name+range string '.bold.yellow[{}]')"_styled,
                    range_part->value);
        }
        if (proj_man) {
            dds_log(error,
                    "  (While reading project manifest from [.bold.yellow[{}]]"_styled,
                    proj_man->value);
        }
        dds_log(error, "  Error: .bold.red[{}]"_styled, msg.value);
        write_error_marker("invalid-dep-shorthand");
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
    [](e_nonesuch_library missing_lib) {
        dds_log(error, "No such library .bold.red[{}]"_styled, missing_lib.value);
        write_error_marker("no-such-library");
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
                "Exception message from std::system_error: .bold.red[{}]"_styled,
                exc.code().message());
        return 42;
    },
    [](error_base const& exc, boost::leaf::verbose_diagnostic_info const& diag) {
        dds_log(error, "{}", exc.what());
        dds_log(error, "{}", exc.explanation());
        dds_log(error, "Refer: {}", exc.error_reference());
        dds_log(debug, "Additional diagnostic details:\n.br.blue[{}]"_styled, diag);
        return 1;
    },
    [](boost::leaf::verbose_diagnostic_info const& diag) {
        dds_log(critical,
                "An unhandled error arose. .bold.red[THIS IS A DDS BUG!] Info: {}"_styled,
                diag);
        return 42;
    });
}  // namespace

int dds::handle_cli_errors(std::function<int()> fn) noexcept {
    return boost::leaf::try_catch(fn, handlers);
}
