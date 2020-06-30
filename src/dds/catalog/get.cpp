#include "./get.hpp"

#include <dds/catalog/catalog.hpp>
#include <dds/error/errors.hpp>
#include <dds/proc.hpp>

#include <nlohmann/json.hpp>
#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/distance.hpp>
#include <range/v3/numeric/accumulate.hpp>
#include <spdlog/spdlog.h>

using namespace dds;

namespace {

enum operation { move, copy };
void apply_copy(const dds::repo_transform::copy_move& copy, path_ref root, operation op) {
    auto copy_src  = fs::weakly_canonical(root / copy.from);
    auto copy_dest = fs::weakly_canonical(root / copy.to);
    if (fs::relative(copy_src, root).generic_string().find("../") == 0) {
        throw std::runtime_error(
            fmt::format("A copy_src ends up copying from outside the root. (Relative path was "
                        "[{}], resolved path was [{}])",
                        copy.from.string(),
                        copy_src.string()));
    }
    if (fs::relative(copy_dest, root).generic_string().find("../") == 0) {
        throw std::runtime_error(
            fmt::format("A copy_dest ends up copying from outside the root. (Relative path was "
                        "[{}], resolved path was [{}])",
                        copy.from.string(),
                        copy_dest.string()));
    }

    if (fs::is_regular_file(copy_src)) {
        // Just copying a single file? Okay.
        if (op == move) {
            safe_rename(copy_src, copy_dest);
        } else {
            fs::copy_file(copy_src, copy_dest, fs::copy_options::overwrite_existing);
        }
        return;
    }

    auto f_iter = fs::recursive_directory_iterator(copy_src);
    for (auto item : f_iter) {
        auto relpath      = fs::relative(item, copy_src);
        auto matches_glob = [&](auto glob) { return glob.match(relpath.string()); };
        auto included     = ranges::all_of(copy.include, matches_glob);
        auto excluded     = ranges::any_of(copy.exclude, matches_glob);
        if (!included || excluded) {
            continue;
        }

        auto n_components = ranges::distance(relpath);
        if (n_components <= copy.strip_components) {
            continue;
        }

        auto it = relpath.begin();
        std::advance(it, copy.strip_components);
        relpath = ranges::accumulate(it, relpath.end(), fs::path(), std::divides<>());

        auto dest = copy_dest / relpath;
        fs::create_directories(dest.parent_path());
        if (item.is_directory()) {
            fs::create_directories(dest);
        } else {
            if (op == move) {
                safe_rename(item, dest);
            } else {
                fs::copy_file(item, dest, fs::copy_options::overwrite_existing);
            }
        }
    }
}

void apply_remove(const struct dds::repo_transform::remove& rm, path_ref root) {
    const auto item = fs::weakly_canonical(root / rm.path);
    if (fs::relative(item, root).generic_string().find("../") == 0) {
        throw std::runtime_error(fmt::format(
            "A 'remove' ends up removing files from outside the root. (Relative path was "
            "[{}], resolved path was [{}])",
            rm.path.string(),
            item.string()));
    }

    if (!rm.only_matching.empty()) {
        if (!fs::is_directory(item)) {
            throw std::runtime_error(
                fmt::format("A 'remove' item has an 'only_matching' pattern list, but the named "
                            "path is not a directory [{}]",
                            item.string()));
        }
        for (auto glob : rm.only_matching) {
            for (auto rm_item : glob.scan_from(item)) {
                fs::remove_all(rm_item);
            }
        }
    } else {
        fs::remove_all(item);
    }

    if (fs::is_directory(item)) {
    }
}

void apply_transform(const dds::repo_transform& transform, path_ref root) {
    if (transform.copy) {
        apply_copy(*transform.copy, root, copy);
    }
    if (transform.move) {
        apply_copy(*transform.move, root, move);
    }
    if (transform.remove) {
        apply_remove(*transform.remove, root);
    }
}

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
        throw_external_error<errc::git_clone_failure>(
            "Git clone operation failed [Git command: {}] [Exitted {}]:\n{}",
            quote_command(command),
            git_res.retc,
            git_res.output);
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

    for (const auto& tr : listing.transforms) {
        apply_transform(tr, tmpdir.path());
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