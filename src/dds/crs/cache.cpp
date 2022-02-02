#include "./cache.hpp"

#include "./cache_db.hpp"
#include "./remote.hpp"
#include <dds/error/result.hpp>
#include <dds/util/fs/dirscan.hpp>
#include <dds/util/log.hpp>
#include <dds/util/paths.hpp>

#include <boost/leaf/exception.hpp>
#include <fansi/styled.hpp>

#include <neo/memory.hpp>

using namespace dds;
using namespace dds::crs;
using namespace neo::sqlite3::literals;
using namespace fansi::literals;

struct cache::impl {
    fs::path        root_dir;
    unique_database db = unique_database::open((root_dir / "dds-metadata.db").string()).value();
    cache_db        metadata_db = cache_db::open(db);
    file_collector  fcoll       = file_collector::create(db);

    explicit impl(fs::path p)
        : root_dir(p) {
        db.exec_script("PRAGMA journal_mode = WAL"_sql);
    }
};

cache cache::open(path_ref dirpath) {
    fs::create_directories(dirpath);
    return cache{dirpath};
}

cache::cache(path_ref dirpath)
    : _impl(std::make_shared<impl>(dirpath)) {}

cache_db& cache::db() noexcept { return _impl->metadata_db; }

fs::path cache::prefetch(const pkg_id& pid_) {
    auto pid     = pid_;
    auto entries = db().for_package(pid.name, pid.version);
    auto it      = entries.begin();
    if (it == entries.end()) {
        BOOST_LEAF_THROW_EXCEPTION(e_no_such_pkg{pid});
    }
    auto remote = db().get_remote_by_id(it->remote_id);
    if (pid.revision == 0) {
        pid.revision = it->pkg.id.revision;
    }
    auto pkg_dir = _impl->root_dir / "pkgs" / pid.to_string();
    if (fs::exists(pkg_dir)) {
        return pkg_dir;
    }
    neo_assert(invariant,
               remote.has_value(),
               "Unable to get the remote of a just-obtained package entry",
               pid.to_string());
    dds_log(info, "Fetching package .br.cyan[{}]"_styled, pid.to_string());
    crs::pull_pkg_from_remote(pkg_dir, remote->url, pid);
    return pkg_dir;
}

fs::path cache::default_path() noexcept { return dds::dds_cache_dir() / "crs"; }
