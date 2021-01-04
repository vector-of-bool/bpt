#include "../options.hpp"

#include "./pkg_repo_err_handle.hpp"

#include <dds/pkg/db.hpp>
#include <dds/pkg/remote.hpp>
#include <dds/util/result.hpp>

namespace dds::cli::cmd {

static int _pkg_repo_remove(const options& opts) {
    auto cat = opts.open_pkg_db();
    for (auto&& rm_name : opts.pkg.repo.remove.names) {
        dds::remove_remote(cat, rm_name);
    }
    return 0;
}

int pkg_repo_remove(const options& opts) {
    return handle_pkg_repo_remote_errors([&] {
        DDS_E_SCOPE(opts.pkg.repo.subcommand);
        return _pkg_repo_remove(opts);
    });
}

}  // namespace dds::cli::cmd
