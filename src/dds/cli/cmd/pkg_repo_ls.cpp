#include "../options.hpp"

#include "./pkg_repo_err_handle.hpp"

#include <dds/pkg/db.hpp>
#include <dds/pkg/remote.hpp>

#include <neo/sqlite3/iter_tuples.hpp>

namespace dds::cli::cmd {

static int _pkg_repo_ls(const options& opts) {
    auto                       pkg_db = opts.open_catalog();
    neo::sqlite3::database_ref db     = pkg_db.database();

    auto st   = db.prepare("SELECT name, remote_url, db_mtime FROM dds_pkg_remotes");
    auto tups = neo::sqlite3::iter_tuples<std::string, std::string, std::optional<std::string>>(st);
    for (auto [name, remote_url, mtime] : tups) {
        fmt::print("Remote '{}':\n", name);
        fmt::print("  Updates URL: {}\n", remote_url);
        if (mtime) {
            fmt::print("  Last Modified: {}\n", *mtime);
        }
        fmt::print("\n");
    }
    return 0;
}

int pkg_repo_ls(const options& opts) {
    return handle_pkg_repo_remote_errors([&] { return _pkg_repo_ls(opts); });
}

}  // namespace dds::cli::cmd
