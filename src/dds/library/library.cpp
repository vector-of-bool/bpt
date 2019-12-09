#include <dds/library/library.hpp>

#include <dds/build/plan/compile_file.hpp>
#include <dds/build/source_dir.hpp>
#include <dds/util/algo.hpp>

#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

using namespace dds;

namespace {

auto collect_pf_sources(path_ref path) {
    auto include_dir = source_directory{path / "include"};
    auto src_dir     = source_directory{path / "src"};

    source_list sources;

    if (include_dir.exists()) {
        if (!fs::is_directory(include_dir.path)) {
            throw std::runtime_error("The `include` at the root of the project is not a directory");
        }
        auto inc_sources = include_dir.sources();
        // Drop any source files we found within `include/`
        erase_if(sources, [&](auto& info) {
            if (info.kind != source_kind::header) {
                spdlog::warn("Source file in `include` will not be compiled: {}",
                             info.path.string());
                return true;
            }
            return false;
        });
        extend(sources, inc_sources);
    }

    if (src_dir.exists()) {
        if (!fs::is_directory(src_dir.path)) {
            throw std::runtime_error("The `src` at the root of the project is not a directory");
        }
        auto src_sources = src_dir.sources();
        extend(sources, src_sources);
    }

    return sources;
}

}  // namespace

library library::from_directory(path_ref lib_dir) {
    auto sources = collect_pf_sources(lib_dir);

    library_manifest man;
    man.name      = lib_dir.filename().string();
    auto man_path = lib_dir / "library.dds";
    if (fs::is_regular_file(man_path)) {
        man = library_manifest::load_from_file(man_path);
    }

    auto lib = library(lib_dir, std::move(sources), std::move(man));

    return lib;
}

fs::path library::public_include_dir() const noexcept {
    auto inc_dir = include_dir();
    if (inc_dir.exists()) {
        return inc_dir.path;
    }
    return src_dir().path;
}

fs::path library::private_include_dir() const noexcept { return src_dir().path; }

shared_compile_file_rules library::base_compile_rules() const noexcept {
    auto                      inc_dir = include_dir();
    auto                      src_dir = this->src_dir();
    shared_compile_file_rules ret;
    if (inc_dir.exists()) {
        ret.include_dirs().push_back(inc_dir.path);
    }
    if (src_dir.exists()) {
        ret.include_dirs().push_back(src_dir.path);
    }
    return ret;
}

auto has_library_dirs
    = [](path_ref dir) { return fs::exists(dir / "src") || fs::exists(dir / "include"); };

std::vector<library> dds::collect_libraries(path_ref root) {
    std::vector<library> ret;
    if (has_library_dirs(root)) {
        ret.emplace_back(library::from_directory(root));
    }

    auto pf_libs_dir = root / "libs";

    if (fs::is_directory(pf_libs_dir)) {
        extend(ret,
               fs::directory_iterator(pf_libs_dir)            //
                   | ranges::views::filter(has_library_dirs)  //
                   | ranges::views::transform([&](auto p) { return library::from_directory(p); }));
    }
    return ret;
}