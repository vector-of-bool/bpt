#include "../options.hpp"

#include "./pkg_repo_err_handle.hpp"

#include <dds/pkg/db.hpp>
#include <dds/remote/remote.hpp>

namespace dds::cli::cmd {

static int _pkg_repo_update(const options& opts) {
    update_all_remotes(opts.open_catalog().database());
    return 0;
}

int pkg_repo_update(const options& opts) {
    return handle_pkg_repo_remote_errors([&] { return _pkg_repo_update(opts); });
}

}  // namespace dds::cli::cmd
