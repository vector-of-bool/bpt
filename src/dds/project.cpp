#include <dds/project.hpp>

#include <dds/source.hpp>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

using namespace dds;

namespace {

bool has_library_dirs(path_ref p) { return fs::exists(p / "src") || fs::exists(p / "include"); }

std::vector<library> collect_submodules(path_ref pf_libs_dir) {
    if (!fs::exists(pf_libs_dir)) {
        return {};
    }
    using namespace ranges::view;
    return fs::directory_iterator(pf_libs_dir)            //
        | filter(has_library_dirs)                        //
        | transform(DDS_TL(library::from_directory(_1)))  //
        | ranges::to_vector;
}

}  // namespace

project project::from_directory(path_ref pf_dir_path) {
    std::optional<library> main_lib;
    if (has_library_dirs(pf_dir_path)) {
        main_lib = library::from_directory(pf_dir_path);
    }
    return project(std::move(main_lib), collect_submodules(pf_dir_path / "libs"));
}
