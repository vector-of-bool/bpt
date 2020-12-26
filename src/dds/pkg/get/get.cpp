#include "./get.hpp"

#include <dds/error/errors.hpp>
#include <dds/pkg/cache.hpp>
#include <dds/pkg/db.hpp>
#include <dds/util/log.hpp>
#include <dds/util/parallel.hpp>

#include <neo/assert.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

using namespace dds;

namespace {

temporary_sdist do_pull_sdist(const any_remote_pkg& rpkg) {
    auto tmpdir = dds::temporary_dir::create();

    rpkg.get_sdist(tmpdir.path());

    auto         sd_tmp_dir = dds::temporary_dir::create();
    sdist_params params{
        .project_dir = tmpdir.path(),
        .dest_path   = sd_tmp_dir.path(),
        .force       = true,
    };
    auto sd = create_sdist(params);
    return {sd_tmp_dir, sd};
}

}  // namespace

temporary_sdist dds::get_package_sdist(const pkg_listing& pkg) {
    auto tsd = do_pull_sdist(pkg.remote_pkg);
    if (!(tsd.sdist.manifest.id == pkg.ident)) {
        throw_external_error<errc::sdist_ident_mismatch>(
            "The package name@version in the generated source distribution does not match the name "
            "listed in the remote listing file (expected '{}', but got '{}')",
            pkg.ident.to_string(),
            tsd.sdist.manifest.id.to_string());
    }
    return tsd;
}

void dds::get_all(const std::vector<pkg_id>& pkgs, pkg_cache& repo, const pkg_db& cat) {
    std::mutex repo_mut;

    auto absent_pkg_infos
        = pkgs  //
        | ranges::views::filter([&](auto pk) {
              std::scoped_lock lk{repo_mut};
              return !repo.find(pk);
          })
        | ranges::views::transform([&](auto id) {
              auto info = cat.get(id);
              neo_assert(invariant, !!info, "No database entry for package id?", id.to_string());
              return *info;
          });

    auto okay = parallel_run(absent_pkg_infos, 8, [&](pkg_listing inf) {
        dds_log(info, "Download package: {}", inf.ident.to_string());
        auto             tsd = get_package_sdist(inf);
        std::scoped_lock lk{repo_mut};
        repo.add_sdist(tsd.sdist, if_exists::throw_exc);
    });

    if (!okay) {
        throw_external_error<errc::dependency_resolve_failure>("Downloading of packages failed.");
    }
}
