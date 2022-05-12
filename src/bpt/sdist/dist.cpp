#include "./dist.hpp"

#include "./error.hpp"
#include <bpt/sdist/root.hpp>

#include <bpt/error/doc_ref.hpp>
#include <bpt/error/errors.hpp>
#include <bpt/error/on_error.hpp>
#include <bpt/error/result.hpp>
#include <bpt/project/project.hpp>
#include <bpt/temp.hpp>
#include <bpt/util/fs/io.hpp>
#include <bpt/util/fs/op.hpp>
#include <bpt/util/fs/shutil.hpp>
#include <bpt/util/json5/parse.hpp>
#include <bpt/util/log.hpp>

#include <boost/leaf/exception.hpp>
#include <fansi/styled.hpp>
#include <libman/parse.hpp>
#include <neo/assert.hpp>
#include <neo/ranges.hpp>
#include <neo/tar/util.hpp>
#include <neo/tl.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>

#include <fstream>

using namespace bpt;
using namespace fansi::literals;

namespace {

void sdist_export_file(path_ref out_root, path_ref in_root, path_ref filepath) {
    auto relpath = fs::relative(filepath, in_root);
    bpt_log(debug, "Export file {}", relpath.string());
    auto dest = out_root / relpath;
    fs::create_directories(fs::absolute(dest).parent_path());
    fs::copy(filepath, dest);
}

void sdist_copy_library(path_ref                 out_root,
                        const sdist&             sd,
                        const crs::library_info& lib,
                        const sdist_params&      params) {
    auto lib_dir = bpt::resolve_path_strong(sd.path / lib.path).value();
    auto inc_dir = source_root{lib_dir / "include"};
    auto src_dir = source_root{lib_dir / "src"};

    auto inc_sources = inc_dir.exists() ? inc_dir.collect_sources() : std::vector<source_file>{};
    auto src_sources = src_dir.exists() ? src_dir.collect_sources() : std::vector<source_file>{};

    using namespace std::views;
    auto all_arr = std::array{all(inc_sources), all(src_sources)};

    bpt_log(info, "sdist: Export library from {}", lib_dir.string());
    fs::create_directories(out_root);
    for (const auto& source : all_arr | join) {
        sdist_export_file(out_root, params.project_dir, source.path);
    }
}

}  // namespace

sdist bpt::create_sdist(const sdist_params& params) {
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
    bpt_log(info, "Source distribution created in {}", dest.string());
    return sdist::from_directory(dest);
}

void bpt::create_sdist_targz(path_ref filepath, const sdist_params& params) {
    if (fs::exists(filepath)) {
        if (!params.force) {
            throw_user_error<errc::sdist_exists>("Destination path '{}' already exists",
                                                 filepath.string());
        }
    }

    auto tempdir = temporary_dir::create();
    bpt_log(debug, "Generating source distribution in {}", tempdir.path().string());
    create_sdist_in_dir(tempdir.path(), params);
    fs::create_directories(fs::absolute(filepath).parent_path());
    neo::compress_directory_targz(tempdir.path(), filepath);
}

sdist bpt::create_sdist_in_dir(path_ref out, const sdist_params& params) {
    auto in_sd = sdist::from_directory(params.project_dir);
    for (const crs::library_info& lib : in_sd.pkg.libraries) {
        sdist_copy_library(out, in_sd, lib, params);
    }
    in_sd.pkg.id.revision = params.revision;

    fs::create_directories(out);
    bpt::write_file(out / "pkg.json", in_sd.pkg.to_json(2));
    return sdist::from_directory(out);
}

sdist sdist::from_directory(path_ref where) {
    BPT_E_SCOPE(e_sdist_from_directory{where});
    crs::package_info meta;

    auto       pkg_json      = where / "pkg.json";
    auto       bpt_yaml      = where / "bpt.yaml";
    const bool have_pkg_json = bpt::file_exists(pkg_json);
    const bool have_bpt_yaml = bpt::file_exists(bpt_yaml);

    if (have_pkg_json) {
        if (have_bpt_yaml) {
            bpt_log(
                warn,
                "Directory has both [.cyan[{}]] and [.cyan[{}]] (The .bold.cyan[pkg`.json] file will be preferred)"_styled,
                pkg_json.string(),
                bpt_yaml.string());
        }
        BPT_E_SCOPE(crs::e_pkg_json_path{pkg_json});
        auto data = bpt::parse_json5_file(pkg_json);
        meta      = crs::package_info::from_json_data(data);
    } else {
        auto proj = project::open_directory(where);
        if (!proj.manifest.has_value()) {
            BOOST_LEAF_THROW_EXCEPTION(
                make_user_error<errc::invalid_pkg_filesystem>(
                    "No pkg.json nor project manifest in the project directory"),
                e_missing_pkg_json{pkg_json},
                e_missing_project_yaml{where / "bpt.yaml"},
                BPT_ERR_REF("invalid-pkg-filesystem"));
        }
        meta = proj.manifest->as_crs_package_meta();
    }
    return sdist{meta, where};
}
