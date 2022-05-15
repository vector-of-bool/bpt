#include "./error_handler.hpp"
#include "./options.hpp"

#include <bpt/crs/error.hpp>
#include <bpt/crs/info/package.hpp>
#include <bpt/deps.hpp>
#include <bpt/error/doc_ref.hpp>
#include <bpt/error/errors.hpp>
#include <bpt/error/exit.hpp>
#include <bpt/error/toolchain.hpp>
#include <bpt/project/dependency.hpp>
#include <bpt/project/error.hpp>
#include <bpt/project/spdx.hpp>
#include <bpt/sdist/error.hpp>
#include <bpt/toolchain/errors.hpp>
#include <bpt/usage_reqs.hpp>
#include <bpt/util/compress.hpp>
#include <bpt/util/fs/io.hpp>
#include <bpt/util/http/error.hpp>
#include <bpt/util/http/pool.hpp>
#include <bpt/util/json5/error.hpp>
#include <bpt/util/log.hpp>
#include <bpt/util/result.hpp>
#include <bpt/util/signal.hpp>
#include <bpt/util/url.hpp>
#include <bpt/util/yaml/errors.hpp>

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

using namespace bpt;
using namespace fansi::literals;

using boost::leaf::catch_;

namespace {

auto handlers = std::tuple(  //
    [](e_sdist_from_directory       sdist_dirpath,
       e_json_parse_error           error,
       const std::filesystem::path* maybe_fpath) {
        bpt_log(
            error,
            "Invalid metadata file while opening source distribution/project in [.bold.yellow[{}]]"_styled,
            sdist_dirpath.value.string());
        bpt_log(error, "Invalid JSON file: .bold.red[{}]"_styled, error.value);
        if (maybe_fpath) {
            bpt_log(error, "  (While reading from [{}])", maybe_fpath->string());
        }
        write_error_marker("package-json5-parse-error");
        return 1;
    },
    [](e_sdist_from_directory       sdist_dirpath,
       bpt::e_yaml_parse_error      error,
       const std::filesystem::path* maybe_fpath) {
        bpt_log(
            error,
            "Invalid metadata file while opening source distribution/project in [.bold.yellow[{}]]"_styled,
            sdist_dirpath.value.string());
        bpt_log(error, "Invalid YAML file: .bold.red[{}]"_styled, error.value);
        if (maybe_fpath) {
            bpt_log(error, "  (While reading from [{}])", maybe_fpath->string());
        }
        write_error_marker("package-yaml-parse-error");
        return 1;
    },
    [](e_sdist_from_directory, e_parse_project_manifest_path bpt_yaml, e_bad_bpt_yaml_key badkey) {
        bpt_log(error,
                "Error loading project info from [.bold.yellow[{}]]"_styled,
                bpt_yaml.value.string());
        badkey.log_error("Unknown project property '.bold.red[{}]'"_styled);
        return 1;
    },
    [](e_parse_dependency_manifest_path deps_json, e_bad_deps_json_key badkey) {
        bpt_log(error,
                "Error loading dependency info from [.bold.yellow[{}]]"_styled,
                deps_json.value.string());
        badkey.log_error("Unknown property '.bold.red[{}]'"_styled);
        return 1;
    },
    [](const semester::walk_error&    exc,
       e_sdist_from_directory         dir,
       e_parse_project_manifest_path* bpt_yaml,
       crs::e_pkg_json_path*          pkg_json) {
        if (pkg_json) {
            bpt_log(error,
                    "Error loading package info from [.bold.yellow[{}]]: .bold.red[{}]"_styled,
                    pkg_json->value.string(),
                    exc.what());
            write_error_marker("invalid-pkg-json");
        } else if (bpt_yaml) {
            bpt_log(error,
                    "Error loading project info from [.bold.yellow[{}]]: .bold.red[{}]"_styled,
                    bpt_yaml->value.string(),
                    exc.what());
            write_error_marker("invalid-pkg-yaml");
        } else {
            bpt_log(error,
                    "Error parsing data in directory [.bold.yellow[{}]]: .bold.red[{}]"_styled,
                    dir.value.string(),
                    exc.what());
        }
        return 1;
    },
    [](const neo::url_validation_error& exc,
       e_sdist_from_directory,
       e_parse_project_manifest_path bpt_yaml,
       e_url_string                  str) {
        bpt_log(
            error,
            "Error while parsing URL string '.bold.yellow[{}]' in [.bold.yellow[{}]]: .bold.red[{}]"_styled,
            str.value,
            bpt_yaml.value.string(),
            exc.what());
        return 1;
    },
    [](bpt::e_bad_spdx_expression err,
       bpt::e_spdx_license_str    spdx_str,
       e_sdist_from_directory,
       e_parse_project_manifest_path bpt_yaml) {
        bpt_log(error,
                "Invalid SPDX license expression '.bold.yellow[{}]': .bold.red[{}]"_styled,
                spdx_str.value,
                err.value);
        bpt_log(error,
                "  (While reading project manifest from [.bold.yellow[{}]]"_styled,
                bpt_yaml.value.string());
        write_error_marker("invalid-spdx");
        return 1;
    },
    [](user_cancelled) {
        bpt_log(critical, "Operation cancelled by the user");
        return 2;
    },
    [](const std::system_error& e, neo::url url, http_response_info) {
        bpt_log(error,
                "An error occurred while downloading [.bold.red[{}]]: {}"_styled,
                url.to_string(),
                e.code().message());
        return 1;
    },
    [](const std::system_error& e, network_origin origin, neo::url* url) {
        bpt_log(error,
                "Network error communicating with .bold.red[{}://{}:{}]: {}"_styled,
                origin.protocol,
                origin.hostname,
                origin.port,
                e.code().message());
        if (url) {
            bpt_log(error, "  (While accessing URL [.bold.red[{}]])"_styled, url->to_string());
        }
        return 1;
    },
    [](const std::system_error& err, e_loading_toolchain, bpt::e_toolchain_filepath* tc_file) {
        bpt_log(error, "Failed to load toolchain: .br.yellow[{}]"_styled, err.code().message());
        if (tc_file) {
            bpt_log(error, "  (While loading from file [.bold.red[{}]])"_styled, tc_file->value);
        }
        write_error_marker("bad-toolchain");
        return 1;
    },
    [](e_name_str                           badname,
       invalid_name_reason                  why,
       e_parse_dep_range_shorthand_string   depstr,
       e_parse_project_manifest_path const* prman_path) {
        bpt_log(
            error,
            "Invalid package name '.bold.red[{}]' in dependency string '.br.red[{}]': .br.yellow[{}]"_styled,
            badname.value,
            depstr.value,
            invalid_name_reason_str(why));
        if (prman_path) {
            bpt_log(error,
                    "  (While reading project manifest from [.bold.yellow[{}]])"_styled,
                    prman_path->value);
        }
        write_error_marker("invalid-pkg-dep-name");
        return 1;
    },
    [](e_name_str badname, invalid_name_reason why, e_parse_project_manifest_path* prman_path) {
        bpt_log(error,
                "Invalid name string '.bold.red[{}]': .br.yellow[{}]"_styled,
                badname.value,
                invalid_name_reason_str(why));
        if (prman_path) {
            bpt_log(error,
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
        bpt_log(error,
                "Invalid dependency shorthand string '.bold.yellow[{}]'"_styled,
                given.value);
        if (range_part) {
            bpt_log(error,
                    "  (While parsing name+range string '.bold.yellow[{}]')"_styled,
                    range_part->value);
        }
        if (proj_man) {
            bpt_log(error,
                    "  (While reading project manifest from [.bold.yellow[{}]]"_styled,
                    proj_man->value);
        }
        bpt_log(error, "  Error: .bold.red[{}]"_styled, msg.value);
        write_error_marker("invalid-dep-shorthand");
        return 1;
    },
    [](const semver::invalid_version&            exc,
       e_parse_dep_shorthand_string              given,
       e_parse_dep_range_shorthand_string const* range_part,
       e_parse_project_manifest_path const*      proj_man) {
        bpt_log(error,
                "Invalid dependency shorthand string '.bold.yellow[{}]'"_styled,
                given.value);
        if (range_part) {
            bpt_log(error,
                    "  (While parsing name+range string '.bold.yellow[{}]')"_styled,
                    range_part->value);
        }
        if (proj_man) {
            bpt_log(error,
                    "  (While reading project manifest from [.bold.yellow[{}]]"_styled,
                    proj_man->value);
        }
        bpt_log(error, "  Error: .bold.red[{}]"_styled, exc.what());
        write_error_marker("invalid-dep-shorthand");
        return 1;
    },

    [](e_nonesuch_library missing_lib) {
        bpt_log(error,
                "No such library .bold.red[{}] in package .bold.red[{}]"_styled,
                missing_lib.value.name,
                missing_lib.value.namespace_);
        write_error_marker("no-such-library");
        return 1;
    },
    [](bpt::e_exit ex, boost::leaf::verbose_diagnostic_info const& info) {
        bpt_log(trace, "Additional error information: {}", info);
        return ex.value;
    },
    [](bpt::e_human_message                        message,
       bpt::e_doc_ref                              docref,
       boost::leaf::verbose_diagnostic_info const& diag,
       bpt::e_error_marker const*                  marker) {
        bpt_log(error, "Error: .bold.red[{}]"_styled, message.value);
        bpt_log(error, "Refer: .bold.yellow[https://dds`.pizza/docs/{}]"_styled, docref.value);
        bpt_log(debug, "Additional diagnostic objects:\n.blue[{}]"_styled, diag);
        if (marker) {
            bpt::write_error_marker(marker->value);
        }
        return 1;
    },
    [](catch_<neo::sqlite3::error> exc, boost::leaf::verbose_diagnostic_info const& diag) {
        bpt_log(critical,
                "An unhandled SQLite error arose. .bold.red[THIS IS A BPT BUG!] Info: {}"_styled,
                diag);
        bpt_log(critical,
                "Exception message from neo::sqlite3::error: .bold.red[{}]"_styled,
                exc.matched.what());
        return 42;
    },
    [](const std::system_error& exc, boost::leaf::verbose_diagnostic_info const& diag) {
        bpt_log(
            critical,
            "An unhandled std::system_error arose. .bold.red[THIS IS A BPT BUG!] Info: {}"_styled,
            diag);
        bpt_log(critical,
                "Exception message from std::system_error: .bold.red[{}]"_styled,
                exc.code().message());
        return 42;
    },
    [](error_base const& exc, boost::leaf::verbose_diagnostic_info const& diag) {
        bpt_log(error, "{}", exc.what());
        bpt_log(error, "{}", exc.explanation());
        bpt_log(error, "Refer: {}", exc.error_reference());
        bpt_log(debug, "Additional diagnostic details:\n.br.blue[{}]"_styled, diag);
        return 1;
    },
    [](boost::leaf::verbose_diagnostic_info const& diag) {
        bpt_log(critical,
                "An unhandled error arose. .bold.red[THIS IS A BPT BUG!] Info: {}"_styled,
                diag);
        return 42;
    });
}  // namespace

int bpt::handle_cli_errors(std::function<int()> fn) noexcept {
    return boost::leaf::try_catch(fn, handlers);
}
