#include "./dist.hpp"

#include <dds/error/errors.hpp>
#include <dds/library/root.hpp>
#include <dds/temp.hpp>
#include <dds/util/fs.hpp>

#include <libman/parse.hpp>

#include <range/v3/algorithm/sort.hpp>
#include <range/v3/view/filter.hpp>

#include <spdlog/spdlog.h>

using namespace dds;

namespace {

void sdist_export_file(path_ref out_root, path_ref in_root, path_ref filepath) {
    auto relpath = fs::relative(filepath, in_root);
    spdlog::info("Export file {}", relpath.string());
    auto dest = out_root / relpath;
    fs::create_directories(dest.parent_path());
    fs::copy(filepath, dest);
}

void sdist_copy_library(path_ref out_root, const library_root& lib, const sdist_params& params) {
    auto sources_to_keep =  //
        lib.all_sources()   //
        | ranges::views::filter([&](const source_file& sf) {
              if (sf.kind == source_kind::app && params.include_apps) {
                  return true;
              }
              if (sf.kind == source_kind::source || sf.kind == source_kind::header) {
                  return true;
              }
              if (sf.kind == source_kind::test && params.include_tests) {
                  return true;
              }
              return false;
          })  //
        | ranges::to_vector;

    ranges::sort(sources_to_keep, std::less<>(), [](auto&& s) { return s.path; });

    auto lib_man_path = library_manifest::find_in_directory(lib.path());
    if (!lib_man_path) {
        throw_user_error<errc::invalid_lib_filesystem>(
            "Each library root in a source distribution requires a library manifest (Expected a "
            "library manifest in [{}])",
            lib.path().string());
    }
    sdist_export_file(out_root, params.project_dir, *lib_man_path);

    spdlog::info("sdist: Export library from {}", lib.path().string());
    fs::create_directories(out_root);
    for (const auto& source : sources_to_keep) {
        sdist_export_file(out_root, params.project_dir, source.path);
    }
}

}  // namespace

sdist dds::create_sdist(const sdist_params& params) {
    auto dest = fs::absolute(params.dest_path);
    if (fs::exists(dest)) {
        if (!params.force) {
            throw_user_error<errc::sdist_exists>("Destination path '{}' already exists",
                                                 dest.string());
        }
    }

    auto tempdir = temporary_dir::create();
    create_sdist_in_dir(tempdir.path(), params);
    if (fs::exists(dest) && params.force) {
        fs::remove_all(dest);
    }
    fs::create_directories(dest.parent_path());
    safe_rename(tempdir.path(), dest);
    spdlog::info("Source distribution created in {}", dest.string());
    return sdist::from_directory(dest);
}

sdist dds::create_sdist_in_dir(path_ref out, const sdist_params& params) {
    auto libs = collect_libraries(params.project_dir);

    for (const library_root& lib : libs) {
        sdist_copy_library(out, lib, params);
    }

    auto man_path = package_manifest::find_in_directory(params.project_dir);
    if (!man_path) {
        throw_user_error<errc::invalid_pkg_filesystem>(
            "Creating a source distribution requires a package.json5 file for the project "
            "(Expected manifest in [{}])",
            params.project_dir.string());
    }

    auto pkg_man = package_manifest::load_from_file(*man_path);
    sdist_export_file(out, params.project_dir, *man_path);
    spdlog::info("Generated export as {}", pkg_man.pkg_id.to_string());
    return sdist::from_directory(out);
}

sdist sdist::from_directory(path_ref where) {
    auto pkg_man = package_manifest::load_from_directory(where);
    // Code paths should only call here if they *know* that the sdist is valid
    assert(pkg_man.has_value());
    return sdist{pkg_man.value(), where};
}