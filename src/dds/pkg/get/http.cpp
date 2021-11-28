#include "./http.hpp"

#include <dds/error/errors.hpp>
#include <dds/temp.hpp>
#include <dds/util/fs/io.hpp>
#include <dds/util/http/pool.hpp>
#include <dds/util/log.hpp>
#include <dds/util/result.hpp>

#include <neo/io/stream/buffers.hpp>
#include <neo/io/stream/file.hpp>
#include <neo/tar/util.hpp>
#include <neo/url.hpp>
#include <neo/url/query.hpp>

#include <fstream>

using namespace dds;

void http_remote_pkg::do_get_raw(path_ref dest) const {
    dds_log(trace, "Downloading remote package via HTTP from [{}]", url.to_string());

    if (url.scheme != "http" && url.scheme != "https") {
        dds_log(error, "Unsupported URL scheme '{}' (in [{}])", url.scheme, url.to_string());
        BOOST_LEAF_THROW_EXCEPTION(user_error<errc::invalid_remote_url>(
                                       "The given URL download is not supported. (Only 'http' and "
                                       "'https' URLs are supported)"),
                                   DDS_E_ARG(e_url_string{url.to_string()}));
    }

    neo_assert(invariant,
               !!url.host,
               "The given URL did not have a host part. This shouldn't be possible... Please file "
               "a bug report.",
               url.to_string());

    // Create a temporary directory in which to download the archive
    auto tdir = dds::temporary_dir::create();
    // For ease of debugging, use the filename from the URL, if possible
    auto fname = fs::path(url.path).filename();
    if (fname.empty()) {
        fname = "dds-download.tmp";
    }
    auto dl_path = tdir.path() / fname;
    fs::create_directories(tdir.path());

    // Download the file!
    {
        auto& pool          = http_pool::thread_local_pool();
        auto [client, resp] = pool.request(url);
        auto dl_file        = neo::file_stream::open(dl_path, neo::open_mode::write);
        client.recv_body_into(resp, neo::stream_io_buffers{dl_file});
    }

    fs::create_directories(fs::absolute(dest));
    dds_log(debug, "Expanding downloaded package archive into [{}]", dest.string());
    std::ifstream infile{dl_path, std::ios::binary};
    try {
        neo::expand_directory_targz(
            neo::expand_options{
                .destination_directory = dest,
                .input_name            = dl_path.string(),
                .strip_components      = this->strip_n_components,
            },
            infile);
    } catch (const std::runtime_error& err) {
        throw_external_error<errc::invalid_remote_url>(
            "The file downloaded from [{}] failed to extract (Inner error: {})",
            url.to_string(),
            err.what());
    }
}

http_remote_pkg http_remote_pkg::from_url(const neo::url& url) {
    neo_assert(expects,
               url.scheme == neo::oper::any_of("http", "https"),
               "Invalid URL for an HTTP remote",
               url.to_string());

    neo::url ret_url = url;
    if (url.fragment) {
        dds_log(warn,
                "Fragment '{}' in URL [{}] will have no effect",
                *url.fragment,
                url.to_string());
        ret_url.fragment.reset();
    }

    ret_url.query = {};

    unsigned n_strpcmp = 0;

    if (url.query) {
        std::string query_acc;

        neo::basic_query_string_view qsv{*url.query};
        for (auto qstr : qsv) {
            if (qstr.key_raw() == "__dds_strpcmp") {
                n_strpcmp = static_cast<unsigned>(std::stoul(qstr.value_decoded()));
            } else {
                if (!query_acc.empty()) {
                    query_acc.push_back(';');
                }
                query_acc.append(qstr.string());
            }
        }
        if (!query_acc.empty()) {
            ret_url.query = query_acc;
        }
    }

    return {ret_url, n_strpcmp};
}

neo::url http_remote_pkg::do_to_url() const {
    auto ret_url = url;
    if (strip_n_components != 0) {
        auto strpcmp_param = fmt::format("__dds_strpcmp={}", strip_n_components);
        if (ret_url.query) {
            *ret_url.query += ";" + strpcmp_param;
        } else {
            ret_url.query = strpcmp_param;
        }
    }
    return ret_url;
}
