#include "./sdist.hpp"

#include <dds/project.hpp>
#include <dds/temp.hpp>

#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/view/filter.hpp>

#include <spdlog/spdlog.h>

using namespace dds;

namespace {

void sdist_copy_library(path_ref out_root, const library& lib, const sdist_params& params) {
    auto sources_to_keep =  //
        lib.sources()       //
        | ranges::view::filter([&](const source_file& sf) {
              if (sf.kind == source_kind::source || sf.kind == source_kind::header) {
                  return true;
              }
              if (sf.kind == source_kind::app && params.include_apps) {
                  return true;
              }
              if (sf.kind == source_kind::test && params.include_tests) {
                  return true;
              }
              return false;
          });

    spdlog::info("Export library source from {}", lib.base_dir().string());
    for (const auto& source : sources_to_keep) {
        auto relpath = fs::relative(source.path, lib.base_dir());
        spdlog::info("Copy source file {}", relpath.string());
        auto dest = out_root / relpath;
        fs::create_directories(dest.parent_path());
        fs::copy(source.path, dest);
    }
}

}  // namespace

void dds::create_sdist(const sdist_params& params) {
    auto dest = params.dest_path;
    if (fs::exists(dest)) {
        if (!params.force) {
            throw std::runtime_error(
                fmt::format("Destination path '{}' already exists", dest.string()));
        }
    }

    auto tempdir = temporary_dir::create();
    create_sdist_in_dir(tempdir.path(), params);
    if (fs::exists(dest) && params.force) {
        fs::remove_all(params.dest_path);
    }
    fs::create_directories(params.dest_path.parent_path());
    safe_rename(tempdir.path(), params.dest_path);
    spdlog::info("Source distribution created in {}", params.dest_path.string());
}

void dds::create_sdist_in_dir(path_ref out, const sdist_params& params) {
    auto project = project::from_directory(params.project_dir);
    if (project.main_library()) {
        sdist_copy_library(out, *project.main_library(), params);
    }
}
