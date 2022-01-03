#include "./dist.hpp"

#include "./error.hpp"
#include <dds/sdist/root.hpp>

#include <dds/error/errors.hpp>
#include <dds/pkg/get/http.hpp>
#include <dds/project/project.hpp>
#include <dds/temp.hpp>
#include <dds/util/fs/io.hpp>
#include <dds/util/fs/op.hpp>
#include <dds/util/fs/shutil.hpp>
#include <dds/util/json5/parse.hpp>
#include <dds/util/log.hpp>

#include <libman/parse.hpp>

#include <neo/assert.hpp>
#include <neo/ranges.hpp>
#include <neo/tar/util.hpp>
#include <neo/tl.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>

#include <fstream>

using namespace dds;

namespace {

void sdist_export_file(path_ref out_root, path_ref in_root, path_ref filepath) {
    auto relpath = fs::relative(filepath, in_root);
    dds_log(debug, "Export file {}", relpath.string());
    auto dest = out_root / relpath;
    fs::create_directories(fs::absolute(dest).parent_path());
    fs::copy(filepath, dest);
}

void sdist_copy_library(path_ref                 out_root,
                        const sdist&             sd,
                        const crs::library_meta& lib,
                        const sdist_params&      params) {
    auto lib_dir = dds::resolve_path_strong(sd.path / lib.path).value();
    auto inc_dir = source_root{lib_dir / "include"};
    auto src_dir = source_root{lib_dir / "src"};

    auto inc_sources = inc_dir.exists() ? inc_dir.collect_sources() : std::vector<source_file>{};
    auto src_sources = src_dir.exists() ? src_dir.collect_sources() : std::vector<source_file>{};

    using namespace std::views;
    auto all_arr = std::array{all(inc_sources), all(src_sources)};

    dds_log(info, "sdist: Export library from {}", lib_dir.string());
    fs::create_directories(out_root);
    for (const auto& source : all_arr | join) {
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
    fs::create_directories(fs::absolute(dest).parent_path());
    move_file(tempdir.path(), dest).value();
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
    fs::create_directories(fs::absolute(filepath).parent_path());
    neo::compress_directory_targz(tempdir.path(), filepath);
}

sdist dds::create_sdist_in_dir(path_ref out, const sdist_params& params) {
    auto in_sd = sdist::from_directory(params.project_dir);
    for (const crs::library_meta& lib : in_sd.pkg.libraries) {
        sdist_copy_library(out, in_sd, lib, params);
    }

    fs::create_directories(out);
    dds::write_file(out / "pkg.json", in_sd.pkg.to_json(2));
    return sdist::from_directory(out);
}

sdist sdist::from_directory(path_ref where) {
    DDS_E_SCOPE(e_sdist_from_directory{where});
    crs::package_meta meta;

    auto pkg_json = where / "pkg.json";
    if (dds::file_exists(pkg_json)) {
        DDS_E_SCOPE(crs::e_pkg_json_path{pkg_json});
        auto data = dds::parse_json5_file(pkg_json);
        meta      = crs::package_meta::from_json_data(data);
    } else {
        auto proj = project::open_directory(where);
        if (!proj.manifest.has_value()) {
            BOOST_LEAF_THROW_EXCEPTION(
                make_user_error<errc::invalid_pkg_filesystem>(
                    "No pkg.json nor project manifest in the project directory"),
                e_missing_pkg_json{pkg_json},
                e_missing_project_json5{where / "project.json5"});
        }
        meta = proj.manifest->as_crs_package_meta();
    }
    return sdist{meta, where};
}

temporary_sdist dds::expand_sdist_targz(path_ref targz_path) {
    neo_assertion_breadcrumbs("Expanding sdist targz file", targz_path.string());
    auto infile = dds::open_file(targz_path, std::ios::binary | std::ios::in);
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
    auto remote  = http_remote_pkg::from_url(neo::url::parse(url_str));
    auto tempdir = temporary_dir::create();
    remote.get_raw_directory(tempdir.path());
    return {tempdir, sdist::from_directory(tempdir.path())};
}
