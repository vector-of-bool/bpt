#include "./http.hpp"

#include <dds/error/errors.hpp>
#include <dds/temp.hpp>
#include <dds/util/http/pool.hpp>
#include <dds/util/log.hpp>

#include <neo/io/stream/buffers.hpp>
#include <neo/io/stream/file.hpp>
#include <neo/tar/util.hpp>
#include <neo/url.hpp>
#include <neo/url/query.hpp>

using namespace dds;

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

    http_pool pool;
    auto [client, resp] = pool.request(url);
    auto dl_file        = neo::file_stream::open(dl_path, neo::open_mode::write);
    client.recv_body_into(resp, neo::stream_io_buffers{dl_file});

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

    // IF we are a dds+ URL, strip_components should be zero, and give the url a plain
    // HTTP/HTTPS scheme
    if (url.scheme.starts_with("dds+")) {
        url.scheme       = url.scheme.substr(4);
        strip_components = 0;
    } else if (url.scheme.ends_with("+dds")) {
        url.scheme.erase(url.scheme.end() - 3);
        strip_components = 0;
    } else {
        // Leave the URL as-is
    }

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
