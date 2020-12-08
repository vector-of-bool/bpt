#include <dds/library/root.hpp>

#include <dds/build/plan/compile_file.hpp>
#include <dds/error/errors.hpp>
#include <dds/source/root.hpp>
#include <dds/util/algo.hpp>
#include <dds/util/log.hpp>

#include <neo/ref.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

using namespace dds;

namespace {

auto collect_pf_sources(path_ref path) {
    dds_log(debug, "Scanning for sources in {}", path.string());
    auto include_dir = source_root{path / "include"};
    auto src_dir     = source_root{path / "src"};

    source_list sources;

    if (include_dir.exists()) {
        if (!fs::is_directory(include_dir.path)) {
            throw_user_error<errc::invalid_lib_filesystem>(
                "The `include` at the library root [in {}] is a non-directory file", path.string());
        }
        auto inc_sources = include_dir.collect_sources();
        // Drop any source files we found within `include/`
        erase_if(sources, [&](auto& info) {
            if (info.kind != source_kind::header) {
                dds_log(warn,
                        "Source file in `include` will not be compiled: {}",
                        info.path.string());
                return true;
            }
            return false;
        });
        extend(sources, inc_sources);
    }

    if (src_dir.exists()) {
        if (!fs::is_directory(src_dir.path)) {
            throw_user_error<errc::invalid_lib_filesystem>(
                "The `src` at the library root [in {}] is a non-directory file", path.string());
        }
        auto src_sources = src_dir.collect_sources();
        extend(sources, src_sources);
    }

    dds_log(debug, "Found {} source files", sources.size());
    return sources;
}

}  // namespace

library_root library_root::from_directory(path_ref lib_dir) {
    assert(lib_dir.is_absolute());
    auto sources = collect_pf_sources(lib_dir);

    library_manifest man;
    man.name   = lib_dir.filename().string();
    auto found = library_manifest::find_in_directory(lib_dir);
    if (found) {
        man = library_manifest::load_from_file(*found);
    }

    auto lib = library_root(lib_dir, std::move(sources), std::move(man));

    return lib;
}

fs::path library_root::public_include_dir() const noexcept {
    auto inc_dir = include_source_root();
    if (inc_dir.exists()) {
        return inc_dir.path;
    }
    return src_source_root().path;
}

fs::path library_root::private_include_dir() const noexcept { return src_source_root().path; }

shared_compile_file_rules library_root::base_compile_rules() const noexcept {
    auto                      inc_dir = include_source_root();
    auto                      src_dir = this->src_source_root();
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

std::vector<library_root> dds::collect_libraries(path_ref root) {
    std::vector<library_root> ret;
    if (has_library_dirs(root)) {
        ret.emplace_back(library_root::from_directory(fs::canonical(root)));
    }

    auto pf_libs_dir = root / "libs";

    if (fs::is_directory(pf_libs_dir)) {
        extend(ret,
               fs::directory_iterator(pf_libs_dir)            //
                   | neo::lref                                //
                   | ranges::views::filter(has_library_dirs)  //
                   | ranges::views::transform(
                       [&](auto p) { return library_root::from_directory(fs::canonical(p)); }));
    }
    return ret;
}
