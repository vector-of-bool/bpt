#include "./remote.hpp"

#include <dds/error/result.hpp>
#include <dds/temp.hpp>
#include <dds/util/fs/io.hpp>
#include <dds/util/fs/shutil.hpp>
#include <dds/util/http/pool.hpp>

#include <neo/scope.hpp>
#include <neo/tar/util.hpp>
#include <neo/ufmt.hpp>

#include <fstream>

using namespace dds;
using namespace dds::crs;

void crs::pull_pkg_ar_from_remote(path_ref dest, neo::url_view from, pkg_id pkg) {
    auto tgz_url = from.normalized() / "pkg" / pkg.name.str
        / neo::ufmt("{}~{}", pkg.version.to_string(), pkg.meta_version) / "pkg.tgz";

    auto tmp = dest.parent_path() / ".dds-download.tmp";
    neo_defer { std::ignore = ensure_absent(tmp); };
    fs::create_directories(dest.parent_path());

    {
        auto& pool   = http_pool::thread_local_pool();
        auto  reqres = pool.request(tgz_url);
        reqres.save_file(tmp);
    }

    dds::ensure_absent(dest).value();
    dds::move_file(tmp, dest).value();
}

void crs::pull_pkg_from_remote(path_ref expand_into, neo::url_view from, pkg_id pkg) {
    auto tmpdir   = dds::temporary_dir::create_in(expand_into.parent_path());
    auto tgz_path = tmpdir.path()
        / neo::ufmt("~{}@{}~{}.tgz", pkg.name.str, pkg.version.to_string(), pkg.meta_version);
    pull_pkg_ar_from_remote(tgz_path, from, pkg);

    auto infile = dds::open_file(tgz_path, std::ios::binary | std::ios::in);
    fs::create_directories(expand_into);
    // try {
    neo::expand_directory_targz(
        neo::expand_options{
            .destination_directory = expand_into,
            .input_name            = tgz_path.string(),
        },
        infile);
    // } catch (const std::runtime_error& err) {
    //     // throw_external_error<errc::invalid_remote_url>(
    //     //     "The file downloaded from [{}] failed to extract (Inner error: {})",
    //     //     tgz.to_string(),
    //     //     err.what());
    // }
}
