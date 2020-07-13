#include "./get.hpp"

#include <dds/catalog/catalog.hpp>
#include <dds/error/errors.hpp>
#include <dds/repo/repo.hpp>
#include <dds/util/parallel.hpp>

#include <neo/assert.hpp>
#include <nlohmann/json.hpp>
#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/distance.hpp>
#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

using namespace dds;

namespace {

temporary_sdist do_pull_sdist(const package_info& listing, std::monostate) {
    neo_assert_always(
        invariant,
        false,
        "A package listing in the catalog has no defined remote from which to pull. This "
        "shouldn't happen in normal usage. This will occur if the database has been "
        "manually altered, or if DDS has a bug.",
        listing.ident.to_string());
}

temporary_sdist do_pull_sdist(const package_info& listing, const git_remote_listing& git) {
    auto tmpdir = dds::temporary_dir::create();

    spdlog::info("Cloning Git repository: {} [{}] ...", git.url, git.ref);
    git.clone(tmpdir.path());

    for (const auto& tr : git.transforms) {
        tr.apply_to(tmpdir.path());
    }

    spdlog::info("Create sdist from clone ...");
    if (git.auto_lib.has_value()) {
        spdlog::info("Generating library data automatically");

        auto pkg_strm
            = dds::open(tmpdir.path() / "package.json5", std::ios::binary | std::ios::out);
        auto man_json         = nlohmann::json::object();
        man_json["name"]      = listing.ident.name;
        man_json["version"]   = listing.ident.version.to_string();
        man_json["namespace"] = git.auto_lib->namespace_;
        pkg_strm << nlohmann::to_string(man_json);

        auto lib_strm
            = dds::open(tmpdir.path() / "library.json5", std::ios::binary | std::ios::out);
        auto lib_json    = nlohmann::json::object();
        lib_json["name"] = git.auto_lib->name;
        lib_strm << nlohmann::to_string(lib_json);
    }

    sdist_params params;
    params.project_dir = tmpdir.path();
    auto sd_tmp_dir    = dds::temporary_dir::create();
    params.dest_path   = sd_tmp_dir.path();
    params.force       = true;
    auto sd            = create_sdist(params);
    return {sd_tmp_dir, sd};
}

}  // namespace

temporary_sdist dds::get_package_sdist(const package_info& pkg) {
    auto tsd = std::visit([&](auto&& remote) { return do_pull_sdist(pkg, remote); }, pkg.remote);
    if (!(tsd.sdist.manifest.pkg_id == pkg.ident)) {
        throw_external_error<errc::sdist_ident_mismatch>(
            "The package name@version in the generated source distribution does not match the name "
            "listed in the remote listing file (expected '{}', but got '{}')",
            pkg.ident.to_string(),
            tsd.sdist.manifest.pkg_id.to_string());
    }
    return tsd;
}

void dds::get_all(const std::vector<package_id>& pkgs, repository& repo, const catalog& cat) {
    std::mutex repo_mut;

    auto absent_pkg_infos = pkgs  //
        | ranges::views::filter([&](auto pk) {
                                std::scoped_lock lk{repo_mut};
                                return !repo.find(pk);
                            })
        | ranges::views::transform([&](auto id) {
                                auto info = cat.get(id);
                                neo_assert(invariant,
                                           info.has_value(),
                                           "No catalog entry for package id?",
                                           id.to_string());
                                return *info;
                            });

    auto okay = parallel_run(absent_pkg_infos, 8, [&](package_info inf) {
        spdlog::info("Download package: {}", inf.ident.to_string());
        auto             tsd = get_package_sdist(inf);
        std::scoped_lock lk{repo_mut};
        repo.add_sdist(tsd.sdist, if_exists::throw_exc);
    });

    if (!okay) {
        throw_external_error<errc::dependency_resolve_failure>("Downloading of packages failed.");
    }
}
