#include "../options.hpp"

#include "./pkg_repo_err_handle.hpp"

#include <dds/pkg/db.hpp>
#include <dds/remote/remote.hpp>

namespace dds::cli::cmd {

static int _pkg_repo_add(const options& opts) {
    auto cat  = opts.open_catalog();
    auto repo = remote_repository::connect(opts.pkg.repo.add.url);
    repo.store(cat.database());
    if (opts.pkg.repo.add.update) {
        repo.update_catalog(cat.database());
    }
    return 0;
}

int pkg_repo_add(const options& opts) {
    return handle_pkg_repo_remote_errors([&] { return _pkg_repo_add(opts); });
}

}  // namespace dds::cli::cmd
