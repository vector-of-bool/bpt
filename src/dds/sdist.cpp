#include "./sdist.hpp"

#include <dds/project.hpp>
#include <dds/temp.hpp>
#include <dds/util/fs.hpp>

#include <libman/parse.hpp>

#include <browns/md5.hpp>
#include <browns/output.hpp>
#include <neo/buffer.hpp>

#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/view/filter.hpp>

#include <spdlog/spdlog.h>

using namespace dds;

namespace {

void hash_file(std::string_view prefix, path_ref fpath, browns::md5& hash) {
    hash.feed(neo::buffer(prefix));
    std::ifstream infile;
    infile.exceptions(std::ios::failbit | std::ios::badbit);
    using file_iter = std::istreambuf_iterator<char>;
    infile.open(fpath, std::ios::binary);
    assert(infile.good());
    hash.feed(neo::buffer("\nfile_content: "));
    hash.feed(file_iter(infile), file_iter());
}

void sdist_export_file(path_ref out_root, path_ref in_root, path_ref filepath, browns::md5& hash) {
    auto relpath = fs::relative(filepath, in_root);
    spdlog::info("Export file {}", relpath.string());
    hash_file("filepath:" + relpath.generic_string(), filepath, hash);
    auto dest = out_root / relpath;
    fs::create_directories(dest.parent_path());
    fs::copy(filepath, dest);
}

void sdist_copy_library(path_ref            out_root,
                        const library&      lib,
                        const sdist_params& params,
                        browns::md5&        hash) {
    auto sources_to_keep =  //
        lib.sources()       //
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

    auto lib_dds_path = lib.base_dir() / "library.dds";
    if (fs::is_regular_file(lib_dds_path)) {
        sdist_export_file(out_root, params.project_dir, lib_dds_path, hash);
    }

    spdlog::info("sdist: Export library from {}", lib.base_dir().string());
    fs::create_directories(out_root);
    for (const auto& source : sources_to_keep) {
        sdist_export_file(out_root, params.project_dir, source.path, hash);
    }
}

}  // namespace

sdist dds::create_sdist(const sdist_params& params) {
    auto dest = fs::absolute(params.dest_path);
    if (fs::exists(dest)) {
        if (!params.force) {
            throw std::runtime_error(
                fmt::format("Destination path '{}' already exists", dest.string()));
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
    auto project = project::from_directory(params.project_dir);

    browns::md5 md5;

    if (project.main_library()) {
        sdist_copy_library(out, *project.main_library(), params, md5);
    }

    for (const library& submod : project.submodules()) {
        sdist_copy_library(out, submod, params, md5);
    }

    auto man_path = project.root() / "package.dds";
    if (!fs::is_regular_file(man_path)) {
        throw std::runtime_error(fmt::format(
            "Creating a source distribution requires a package.dds file for the project"));
    }
    sdist_export_file(out, params.project_dir, man_path, md5);

    md5.pad();
    auto hash_str = browns::format_digest(md5.digest());
    spdlog::info("Generated export as {}-{}", project.manifest().name, hash_str);

    std::vector<lm::pair> pairs;
    pairs.emplace_back("MD5-Hash", hash_str);
    lm::write_pairs(out / "_sdist.dds", pairs);

    return sdist::from_directory(out);
}

sdist sdist::from_directory(path_ref where) {
    auto        pkg_man    = package_manifest::load_from_file(where / "package.dds");
    auto        meta_pairs = lm::parse_file(where / "_sdist.dds");
    std::string hash_str;
    lm::read(fmt::format("Loading source distribution manifest from {}/_sdist.dds", where.string()),
             meta_pairs,
             lm::read_required("MD5-Hash", hash_str),
             lm::reject_unknown());
    return sdist{std::move(pkg_man),
                 browns::parse_digest<browns::md5::digest_type>(hash_str),
                 where};
}