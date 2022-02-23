#include "./cache_util.hpp"

#include "../options.hpp"
#include <dds/crs/cache_db.hpp>
#include <dds/crs/error.hpp>
#include <dds/error/exit.hpp>
#include <dds/error/handle.hpp>
#include <dds/error/marker.hpp>
#include <dds/error/try_catch.hpp>
#include <dds/util/compress.hpp>
#include <dds/util/fs/io.hpp>
#include <dds/util/http/error.hpp>
#include <dds/util/http/pool.hpp>
#include <dds/util/log.hpp>
#include <dds/util/url.hpp>

#include <fansi/styled.hpp>
#include <neo/sqlite3/error.hpp>

using namespace dds;
using namespace fansi::literals;

static void
use_repo(dds::crs::cache_db& meta_db, const cli::options& opts, std::string_view url_or_path) {
    // Convert what may be just a domain name or partial URL into a proper URL:
    auto url = dds::guess_url_from_string(url_or_path);
    // Called by error handler to decide whether to rethrow:
    auto check_cache_after_error = [&] {
        if (opts.repo_sync_mode == cli::repo_sync_mode::always) {
            // We should always sync package listings, so this is a hard error
            sbs::throw_system_exit(1);
        }
        auto rid = meta_db.get_remote(url);
        if (rid.has_value()) {
            // We have prior metadata for the given repository.
            dds_log(warn,
                    "We'll continue by using cached information for .bold.yellow[{}]"_styled,
                    url.to_string());
        } else {
            dds_log(
                error,
                "We have no cached metadata for .bold.red[{}], and were unable to obtain any."_styled,
                url.to_string());
            sbs::throw_system_exit(1);
        }
    };
    dds_leaf_try {
        using m = cli::repo_sync_mode;
        switch (opts.repo_sync_mode) {
        case m::cached_okay:
        case m::always:
            meta_db.sync_remote(url);
            return;
        case m::never:
            return;
        }
    }
    dds_leaf_catch(matchv<dds::e_http_status{404}>,
                   dds::crs::e_sync_remote sync_repo,
                   neo::url                req_url) {
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
        check_cache_after_error();
    }
    dds_leaf_catch(catch_<dds::http_error>,
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
        check_cache_after_error();
    }
    dds_leaf_catch(catch_<neo::sqlite3::error> exc, dds::crs::e_sync_remote sync_repo) {
        dds_log(error,
                "SQLite error while importing data from .br.yellow[{}]: .br.red[{}]"_styled,
                sync_repo.value.to_string(),
                exc.matched.what());
        dds_log(error,
                "It's possible that the downloaded SQLite database is corrupt, invalid, or "
                "incompatible with this version of dds");
        check_cache_after_error();
    }
    dds_leaf_catch(dds::crs::e_sync_remote sync_repo,
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
        check_cache_after_error();
    }
    dds_leaf_catch(const std::system_error& e, neo::url e_url, http_response_info) {
        dds_log(error,
                "An error occurred while downloading [.bold.red[{}]]: {}"_styled,
                e_url.to_string(),
                e.code().message());
        check_cache_after_error();
    }
    dds_leaf_catch(const std::system_error& e, network_origin origin, neo::url const* e_url) {
        dds_log(error,
                "Network error communicating with .bold.red[{}://{}:{}]: {}"_styled,
                origin.protocol,
                origin.hostname,
                origin.port,
                e.code().message());
        if (e_url) {
            dds_log(error, "  (While accessing URL [.bold.red[{}]])"_styled, e_url->to_string());
            check_cache_after_error();
        } else {
            check_cache_after_error();
        }
    };
    meta_db.enable_remote(url);
}

crs::cache cli::open_ready_cache(const cli::options& opts) {
    auto cache
        = dds::crs::cache::open(opts.crs_cache_dir.value_or(dds::crs::cache::default_path()));
    auto& meta_db = cache.db();
    for (auto& r : opts.use_repos) {
        use_repo(meta_db, opts, r);
    }
    if (opts.use_default_repo) {
        use_repo(meta_db, opts, "repo-2.dds.pizza");
    }
    return cache;
}
