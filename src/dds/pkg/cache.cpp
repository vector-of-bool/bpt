#include "./cache.hpp"

#include <dds/error/errors.hpp>
#include <dds/error/handle.hpp>
#include <dds/pkg/db.hpp>
#include <dds/sdist/dist.hpp>
#include <dds/sdist/library/manifest.hpp>
#include <dds/solve/solve.hpp>
#include <dds/util/log.hpp>
#include <dds/util/paths.hpp>
#include <dds/util/string.hpp>

#include <boost/leaf/handle_exception.hpp>
#include <fansi/styled.hpp>
#include <neo/ranges.hpp>
#include <neo/ref.hpp>
#include <neo/tl.hpp>
#include <range/v3/view/concat.hpp>

using namespace dds;
using namespace fansi::literals;
using namespace ranges;

void pkg_cache::_log_blocking(path_ref dirpath) noexcept {
    dds_log(warn, "Another process has the package cache directory locked [{}]", dirpath.string());
    dds_log(warn, "Waiting for cache to be released...");
}

void pkg_cache::_init_cache_dir(path_ref dirpath) noexcept { fs::create_directories(dirpath); }

fs::path pkg_cache::default_local_path() noexcept { return dds_data_dir() / "pkg"; }

namespace {

std::optional<sdist> try_open_sdist_for_directory(path_ref p) noexcept {
    if (starts_with(p.filename().string(), ".")) {
        return std::nullopt;
    }
    return boost::leaf::try_catch(  //
        [&] { return std::make_optional(sdist::from_directory(p)); },
        [&](boost::leaf::catch_<std::runtime_error> exc) {
            dds_log(warn,
                    "Failed to load source distribution from directory '{}': {}",
                    p.string(),
                    exc.value().what());
            return std::nullopt;
        },
        [&](e_package_manifest_path*,
            e_library_manifest_path* lman_path,
            e_pkg_namespace_str*     is_namespace,
            e_pkg_name_str*          is_pkgname,
            e_name_str               bad_name,
            invalid_name_reason      why) {
            dds_log(
                warn,
                "Failed to load a source distribution contained in the package cache directory");
            dds_log(warn,
                    "The invalid source distribution is in [.bold.yellow[{}]]"_styled,
                    p.string());
            if (is_namespace) {
                dds_log(warn,
                        "Invalid package namespace '.bold.yellow[{}]'"_styled,
                        bad_name.value);
            } else if (is_pkgname) {
                dds_log(warn, "Invalid package name '.bold.yellow[{}]'"_styled, bad_name.value);
            } else if (lman_path) {
                dds_log(
                    warn,
                    "Invalid library name '.bold.yellow[{}]' (Defined in [.bold.yellow[{}]])"_styled,
                    bad_name.value,
                    lman_path->value);
            }
            dds_log(warn, "  (.bold.yellow[{}])"_styled, invalid_name_reason_str(why));
            dds_log(warn, "We will ignore this directory and not load it as an available package");
            return std::nullopt;
        },
        leaf_handle_unknown(fmt::format("Failed to load source distribution from directory [{}]",
                                        p.string()),
                            std::nullopt));
}

}  // namespace

pkg_cache pkg_cache::_open_for_directory(bool writeable, path_ref dirpath) {
    sdist_set entries;
    for (auto&& el : fs::directory_iterator(dirpath)) {
        if (auto opt_sdist = try_open_sdist_for_directory(el); opt_sdist.has_value()) {
            entries.emplace(NEO_MOVE(*opt_sdist));
        }
    }

    return {writeable, dirpath, NEO_MOVE(entries)};
}

void pkg_cache::import_sdist(const sdist& sd, if_exists ife_action) {
    neo_assertion_breadcrumbs("Importing sdist archive", sd.manifest.id.to_string());
    if (!_write_enabled) {
        dds_log(critical,
                "DDS attempted to write into a cache that wasn't opened with a write-lock. This "
                "is a hard bug and should be reported. For the safety and integrity of the local "
                "cache, we'll hard-exit immediately.");
        std::terminate();
    }
    auto sd_dest = _root / sd.manifest.id.to_string();
    if (fs::exists(sd_dest)) {
        auto msg = fmt::
            format("Package '{}' (Importing from [{}]) is already available in the local cache",
                   sd.manifest.id.to_string(),
                   sd.path.string());
        if (ife_action == if_exists::throw_exc) {
            throw_user_error<errc::sdist_exists>(msg);
        } else if (ife_action == if_exists::ignore) {
            dds_log(warn, msg);
            return;
        } else {
            dds_log(info, msg + " - Replacing");
        }
    }

    // Create a temporary location where we are creating it
    auto tmp_copy = sd_dest;
    tmp_copy.replace_filename(".tmp-import");
    if (fs::exists(tmp_copy)) {
        fs::remove_all(tmp_copy);
    }
    fs::create_directories(tmp_copy.parent_path());

    // Re-create an sdist from the given sdist. This will prune non-sdist files, rather than just
    // fs::copy_all from the source, which may contain extras.
    sdist_params params{
        .project_dir   = sd.path,
        .dest_path     = tmp_copy,
        .include_apps  = true,
        .include_tests = true,
    };
    create_sdist_in_dir(tmp_copy, params);

    // Swap out the temporary to the final location
    if (fs::exists(sd_dest)) {
        fs::remove_all(sd_dest);
    }
    fs::rename(tmp_copy, sd_dest);
    _sdists.insert(sdist::from_directory(sd_dest));
    dds_log(info, "Source distribution for '{}' successfully imported", sd.manifest.id.to_string());
}

const sdist* pkg_cache::find(const pkg_id& pkg) const noexcept {
    auto found = _sdists.find(pkg);
    if (found == _sdists.end()) {
        return nullptr;
    }
    return &*found;
}

std::vector<pkg_id> pkg_cache::solve(const std::vector<dependency>& deps,
                                     const pkg_db&                  ctlg) const {
    return dds::solve(
        deps,
        [&](std::string_view name) -> std::vector<pkg_id> {
            auto mine = std::views::all(_sdists)  //
                | std::views::filter(NEO_TL(_1.manifest.id.name.str == name))
                | std::views::transform(NEO_TL(_1.manifest.id));
            auto avail = ctlg.by_name(name);
            auto all   = ranges::views::concat(mine, avail) | neo::to_vector;
            std::ranges::sort(all, std::less{});
            std::ranges::unique(all, std::less{});
            return all;
        },
        [&](const pkg_id& pkg_id) {
            auto found = find(pkg_id);
            if (found) {
                return found->manifest.dependencies;
            }
            return ctlg.dependencies_of(pkg_id);
        });
}
