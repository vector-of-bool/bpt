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

#include <boost/leaf.hpp>
#include <fansi/styled.hpp>
#include <libman/library.hpp>
#include <neo/assert.hpp>
#include <neo/ref.hpp>
#include <neo/tl.hpp>
#include <range/v3/action/sort.hpp>
#include <range/v3/action/unique.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

#include <ranges>

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
                    exc.matched.what());
            return std::nullopt;
        },
        [&](e_library_manifest_path lman_path, lm::e_invalid_usage_string usage) {
            dds_log(warn, "Failed to load a source distribution in the package cache directory");
            dds_log(warn,
                    "The source distribution is located in [.bold.yellow[{}]]"_styled,
                    p.string());
            dds_log(
                warn,
                "The source distribution contains a usage requirement of '.bold.red[{}]', which is not a valid 'uses' string"_styled,
                usage.value);
            dds_log(warn,
                    "The invalid 'uses' string is in [.bold.yellow[{}]]"_styled,
                    lman_path.value);
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
    auto entries =
        // Get the top-level `name-version` dirs
        fs::directory_iterator(dirpath)  //
        | neo::lref                      //
        //  Convert each dir into an `sdist` object
        | ranges::views::transform(try_open_sdist_for_directory)  //
        //  Drop items that failed to load
        | ranges::views::filter([](auto&& opt) { return opt.has_value(); })  //
        | ranges::views::transform([](auto&& opt) { return *opt; })          //
        | to<sdist_set>();

    return {writeable, dirpath, std::move(entries)};
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
                                     const pkg_db&                  ctlg,
                                     with_test_deps_t               test_deps_opt,
                                     with_app_deps_t                app_deps_opt) const {
    auto keep_deps = deps  //
        | std::views::filter([&](auto&& dep) {
                         switch (dep.for_kind) {
                         case dep_for_kind::lib:
                             return true;
                         case dep_for_kind::test:
                             return test_deps_opt == with_test_deps;
                         case dep_for_kind::app:
                             return app_deps_opt == with_app_deps;
                         }
                         neo::unreachable();
                     })
        | ranges::to_vector;
    return dds::solve(
        keep_deps,
        [&](std::string_view name) -> std::vector<pkg_id> {
            auto mine = ranges::views::all(_sdists)  //
                | ranges::views::filter(
                            [&](const sdist& sd) { return sd.manifest.id.name.str == name; })
                | ranges::views::transform([](const sdist& sd) { return sd.manifest.id; });
            auto avail = ctlg.by_name(name);
            auto all   = ranges::views::concat(mine, avail) | ranges::to_vector;
            ranges::sort(all, std::less{});
            ranges::unique(all, std::less{});
            return all;
        },
        [&](const pkg_id& pkg_id) {
            auto found = find(pkg_id);
            if (found) {
                return found->manifest.dependencies  //
                    | std::views::filter(NEO_TL(_1.for_kind == dep_for_kind::lib))
                    | ranges::to_vector;
            }
            auto ctlg_deps = ctlg.dependencies_of(pkg_id);
            return ctlg_deps                                                    //
                | std::views::filter(NEO_TL(_1.for_kind == dep_for_kind::lib))  //
                | ranges::to_vector;
        });
}
