#include "./http.hpp"

#include <dds/error/errors.hpp>
#include <dds/http/session.hpp>
#include <dds/temp.hpp>
#include <dds/util/log.hpp>

#include <neo/tar/util.hpp>
#include <neo/url.hpp>
#include <neo/url/query.hpp>

using namespace dds;

namespace {

void http_download_with_redir(neo::url url, path_ref dest) {
    for (auto redir_count = 0;; ++redir_count) {
        auto sess = url.scheme == "https"
            ? http_session::connect_ssl(*url.host, url.port_or_default_port_or(443))
            : http_session::connect(*url.host, url.port_or_default_port_or(80));

        sess.send_head({.method = "GET", .path = url.path});

        auto res_head = sess.recv_head();
        if (res_head.is_error()) {
            dds_log(error,
                    "Received an HTTP {} {} for [{}]",
                    res_head.status,
                    res_head.status_message,
                    url.to_string());
            throw_external_error<errc::http_download_failure>(
                "HTTP error while downloading resource [{}]. Got: HTTP {} '{}'",
                url.to_string(),
                res_head.status,
                res_head.status_message);
        }

        if (res_head.is_redirect()) {
            dds_log(trace,
                    "Received HTTP redirect for [{}]: {} {}",
                    url.to_string(),
                    res_head.status,
                    res_head.status_message);
            if (redir_count == 100) {
                throw_external_error<errc::http_download_failure>("Too many redirects on URL");
            }
            auto loc = res_head.headers.find("Location");
            if (!loc) {
                throw_external_error<errc::http_download_failure>(
                    "HTTP endpoint told us to redirect without sending a 'Location' header "
                    "(Received "
                    "HTTP {} '{}')",
                    res_head.status,
                    res_head.status_message);
            }
            dds_log(debug,
                    "Redirect [{}]: {} {} to [{}]",
                    url.to_string(),
                    res_head.status,
                    res_head.status_message,
                    loc->value);
            auto new_url = neo::url::try_parse(loc->value);
            auto err     = std::get_if<neo::url_parse_error>(&new_url);
            if (err) {
                throw_external_error<errc::http_download_failure>(
                    "Server returned an invalid URL for HTTP redirection [{}]", loc->value);
            }
            url = std::move(std::get<neo::url>(new_url));
            continue;
        }

        // Not a redirect nor an error: Download the body
        dds_log(trace,
                "HTTP {} {} [{}]: Saving to [{}]",
                res_head.status,
                res_head.status_message,
                url.to_string(),
                dest.string());
        sess.recv_body_to_file(res_head, dest);
        break;
    }
}

}  // namespace

void http_remote_listing::pull_source(path_ref dest) const {
    neo::url url;
    try {
        url = neo::url::parse(this->url);
    } catch (const neo::url_validation_error& e) {
        throw_user_error<errc::invalid_remote_url>("Failed to parse the string '{}' as a URL: {}",
                                                   this->url,
                                                   e.what());
    }
    dds_log(trace, "Downloading HTTP remote from [{}]", url.to_string());

    if (url.scheme != "http" && url.scheme != "https") {
        dds_log(error, "Unsupported URL scheme '{}' (in [{}])", url.scheme, url.to_string());
        throw_user_error<errc::invalid_remote_url>(
            "The given URL download is not supported. (Only 'http' URLs are supported, "
            "got '{}')",
            this->url);
    }

    neo_assert(invariant,
               !!url.host,
               "The given URL did not have a host part. This shouldn't be possible... Please file "
               "a bug report.",
               this->url);

    auto tdir     = dds::temporary_dir::create();
    auto url_path = fs::path(url.path);
    auto fname    = url_path.filename();
    if (fname.empty()) {
        fname = "dds-download.tmp";
    }
    auto dl_path = tdir.path() / fname;
    fs::create_directory(dl_path.parent_path());

    http_download_with_redir(url, dl_path);

    neo_assert(invariant,
               fs::is_regular_file(dl_path),
               "HTTP client did not properly download the file??",
               this->url,
               dl_path);

    fs::create_directories(dest);
    dds_log(debug, "Expanding downloaded source distribution into {}", dest.string());
    std::ifstream infile{dl_path, std::ios::binary};
    try {
        neo::expand_directory_targz(
            neo::expand_options{
                .destination_directory = dest,
                .input_name            = dl_path.string(),
                .strip_components      = this->strip_components,
            },
            infile);
    } catch (const std::runtime_error& err) {
        throw_external_error<errc::invalid_remote_url>(
            "The file downloaded from [{}] failed to extract (Inner error: {})",
            this->url,
            err.what());
    }
}

http_remote_listing http_remote_listing::from_url(std::string_view sv) {
    auto url = neo::url::parse(sv);
    dds_log(trace, "Create HTTP remote listing from URL [{}]", sv);

    // Because archives most often have one top-level directory, the default strip-components
    // setting is 'one'
    unsigned int             strip_components = 1;
    std::optional<lm::usage> auto_lib;

    if (url.query) {
        neo::basic_query_string_view qsv{*url.query};
        for (auto qstr : qsv) {
            if (qstr.key_raw() == "dds_lm") {
                auto_lib = lm::split_usage_string(qstr.value_decoded());
            } else if (qstr.key_raw() == "dds_strpcmp") {
                strip_components = static_cast<unsigned>(std::stoul(qstr.value_decoded()));
            } else {
                dds_log(warn, "Unknown query string parameter in package url: '{}'", qstr.string());
            }
        }
    }

    return http_remote_listing{
        {.auto_lib = auto_lib},
        url.to_string(),
        strip_components,
    };
}
