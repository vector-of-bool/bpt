#include "./get.hpp"

#include <dds/catalog/catalog.hpp>
#include <dds/proc.hpp>

#include <spdlog/spdlog.h>

using namespace dds;

namespace {

temporary_sdist do_pull_sdist(const package_info& listing, const git_remote_listing& git) {
    auto tmpdir = dds::temporary_dir::create();
    using namespace std::literals;
    spdlog::info("Cloning Git repository: {} [{}] ...", git.url, git.ref);
    auto command = {"git"s,
                    "clone"s,
                    "--depth=1"s,
                    "--branch"s,
                    git.ref,
                    git.url,
                    tmpdir.path().generic_string()};
    auto git_res = run_proc(command);
    if (!git_res.okay()) {
        throw std::runtime_error(
            fmt::format("Git clone operation failed [Git command: {}] [Exitted {}]:\n{}",
                        quote_command(command),
                        git_res.retc,
                        git_res.output));
    }
    spdlog::info("Create sdist from clone ...");
    if (git.auto_lib.has_value()) {
        spdlog::info("Generating library data automatically");
        auto pkg_strm = dds::open(tmpdir.path() / "package.dds", std::ios::binary | std::ios::out);
        pkg_strm << "Name: " << listing.ident.name << '\n'                    //
                 << "Version: " << listing.ident.version.to_string() << '\n'  //
                 << "Namespace: " << git.auto_lib->namespace_;
        auto lib_strm = dds::open(tmpdir.path() / "library.dds", std::ios::binary | std::ios::out);
        lib_strm << "Name: " << git.auto_lib->name;
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
    if (!(tsd.sdist.manifest.pk_id == pkg.ident)) {
        throw std::runtime_error(fmt::format(
            "The package name@version in the generated sdist does not match the name listed in "
            "the remote listing file (expected '{}', but got '{}')",
            pkg.ident.to_string(),
            tsd.sdist.manifest.pk_id.to_string()));
    }
    return tsd;
}