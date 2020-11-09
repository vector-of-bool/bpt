#include "./dist.hpp"

#include <dds/catalog/remote/http.hpp>
#include <dds/error/errors.hpp>
#include <dds/library/root.hpp>
#include <dds/temp.hpp>
#include <dds/util/fs.hpp>
#include <dds/util/log.hpp>

#include <libman/parse.hpp>

#include <neo/assert.hpp>
#include <neo/tar/util.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>

using namespace dds;

namespace {

void sdist_export_file(path_ref out_root, path_ref in_root, path_ref filepath) {
    auto relpath = fs::relative(filepath, in_root);
    dds_log(debug, "Export file {}", relpath.string());
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

    dds_log(info, "sdist: Export library from {}", lib.path().string());
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
    dds_log(info, "Source distribution created in {}", dest.string());
    return sdist::from_directory(dest);
}

void dds::create_sdist_targz(path_ref filepath, const sdist_params& params) {
    if (fs::exists(filepath)) {
        if (!params.force) {
            throw_user_error<errc::sdist_exists>("Destination path '{}' already exists",
                                                 filepath.string());
        }
    }

    auto tempdir = temporary_dir::create();
    dds_log(debug, "Generating source distribution in {}", tempdir.path().string());
    create_sdist_in_dir(tempdir.path(), params);
    fs::create_directories(filepath.parent_path());
    neo::compress_directory_targz(tempdir.path(), filepath);
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
    dds_log(info, "Generated export as {}", pkg_man.pkg_id.to_string());
    return sdist::from_directory(out);
}

sdist sdist::from_directory(path_ref where) {
    auto pkg_man = package_manifest::load_from_directory(where);
    // Code paths should only call here if they *know* that the sdist is valid
    if (!pkg_man.has_value()) {
        throw_user_error<errc::invalid_pkg_filesystem>(
            "The given directory [{}] does not contain a package manifest file. All source "
            "distribution directories are required to contain a package manifest.",
            where.string());
    }
    return sdist{pkg_man.value(), where};
}

temporary_sdist dds::expand_sdist_targz(path_ref targz_path) {
    auto infile = open(targz_path, std::ios::binary | std::ios::in);
    return expand_sdist_from_istream(infile, targz_path.string());
}

temporary_sdist dds::expand_sdist_from_istream(std::istream& is, std::string_view input_name) {
    auto tempdir = temporary_dir::create();
    dds_log(debug,
            "Expanding source distribution content from [{}] into [{}]",
            input_name,
            tempdir.path().string());
    fs::create_directories(tempdir.path());
    neo::expand_directory_targz({.destination_directory = tempdir.path(), .input_name = input_name},
                                is);
    return {tempdir, sdist::from_directory(tempdir.path())};
}

temporary_sdist dds::download_expand_sdist_targz(std::string_view url_str) {
    auto remote  = http_remote_listing::from_url(url_str);
    auto tempdir = temporary_dir::create();
    remote.pull_source(tempdir.path());
    return {tempdir, sdist::from_directory(tempdir.path())};
}
