#include "../options.hpp"

#include <bpt/crs/error.hpp>
#include <bpt/crs/repo.hpp>
#include <bpt/error/marker.hpp>
#include <bpt/error/try_catch.hpp>
#include <bpt/util/fs/shutil.hpp>

namespace bpt::cli::cmd {

namespace {

int _repo_init(const options& opts) {
    auto try_create = [&] {
        auto repo = bpt::crs::repository::create(opts.repo.repo_dir, opts.repo.init.name);
        bpt_log(info,
                "Created a new CRS repository '{}' in [{}]",
                repo.name(),
                repo.root().string());
        return 0;
    };
    return bpt_leaf_try { return try_create(); }
    bpt_leaf_catch(bpt::crs::e_repo_already_init) {
        switch (opts.if_exists) {
        case if_exists::ignore:
            bpt_log(info,
                    "Directory [{}] already contains a CRS repository",
                    opts.repo.repo_dir.string());
            return 0;
        case if_exists::replace:
            bpt_log(info,
                    "Removing existing repository database [{}]",
                    opts.repo.repo_dir.string());
            bpt::ensure_absent(opts.repo.repo_dir / "repo.db").value();
            return try_create();
        case if_exists::fail:
            throw;
        }
        neo::unreachable();
    };
}
}  // namespace

int repo_init(const options& opts) {
    return bpt_leaf_try { return _repo_init(opts); }
    bpt_leaf_catch(bpt::crs::e_repo_already_init, bpt::crs::e_repo_open_path dirpath) {
        bpt_log(error,
                "Failed to initialize a new repostiory at [{}]: Repository already exists",
                dirpath.value.string());
        write_error_marker("repo-init-already-init");
        return 1;
    }
    bpt_leaf_catch(std::error_code ec, bpt::crs::e_repo_open_path dirpath) {
        bpt_log(error,
                "Error while initializing new repository at [{}]: {}",
                dirpath.value.string(),
                ec.message());
        return 1;
    };
}

}  // namespace bpt::cli::cmd
