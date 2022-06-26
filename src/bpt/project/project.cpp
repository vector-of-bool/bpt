#include "./project.hpp"

#include "./error.hpp"

#include <bpt/error/on_error.hpp>
#include <bpt/error/result.hpp>
#include <bpt/error/try_catch.hpp>
#include <bpt/util/algo.hpp>
#include <bpt/util/fs/io.hpp>
#include <bpt/util/fs/op.hpp>
#include <bpt/util/fs/path.hpp>
#include <bpt/util/json5/convert.hpp>
#include <bpt/util/json5/parse.hpp>
#include <bpt/util/log.hpp>
#include <bpt/util/tl.hpp>
#include <bpt/util/yaml/convert.hpp>
#include <bpt/util/yaml/parse.hpp>

#include <neo/ranges.hpp>
#include <neo/tl.hpp>

using namespace bpt;

project project::open_directory(path_ref path_) {
    BPT_E_SCOPE(e_open_project{path_});
    auto path     = resolve_path_strong(path_).value();
    auto bpt_yaml = path / "bpt.yaml";
    if (!bpt::file_exists(bpt_yaml)) {
        if (bpt::file_exists(path / "bpt.yml")) {
            bpt_log(warn,
                    "There's a [bpt.yml] file in the project directory, but bpt expects a '.yaml' "
                    "file extension. The file [{}] will be ignored.",
                    (path / "bpt.yml").string());
        }
        return project{path, std::nullopt};
    }
    BPT_E_SCOPE(e_parse_project_manifest_path{bpt_yaml});
    auto yml  = bpt::parse_yaml_file(bpt_yaml);
    auto data = bpt::yaml_as_json5_data(yml);
    auto man  = project_manifest::from_json_data(data, path);
    return project{path, man};
}

crs::package_info project_manifest::as_crs_package_meta() const noexcept {
    crs::package_info ret;
    ret.id.name     = name;
    ret.id.version  = version;
    ret.id.revision = 1;

    auto libs = libraries  //
        | std::views::transform([&](project_library lib) {
                    auto deps_as_crs
                        = std::views::transform(&project_dependency::as_crs_dependency);
                    auto deps = lib.lib_dependencies | deps_as_crs | neo::to_vector;
                    extend(deps, root_dependencies | deps_as_crs);
                    auto test_deps = lib.test_dependencies | deps_as_crs | neo::to_vector;
                    extend(test_deps, root_test_dependencies | deps_as_crs);
                    return crs::library_info{
                        .name              = lib.name,
                        .path              = lib.relpath,
                        .intra_using       = lib.intra_using,
                        .intra_test_using  = lib.intra_test_using,
                        .dependencies      = std::move(deps),
                        .test_dependencies = std::move(test_deps),
                    };
                });
    ret.libraries = neo::to_vector(libs);
    if (ret.libraries.empty()) {
        ret.libraries.push_back(crs::library_info{
            .name              = name,
            .path              = ".",
            .intra_using       = {},
            .intra_test_using  = {},
            .dependencies      = {},
            .test_dependencies = {},
        });
        extend(ret.libraries.back().dependencies,
               root_dependencies | std::views::transform(BPT_TL(_1.as_crs_dependency())));
        extend(ret.libraries.back().test_dependencies,
               root_test_dependencies | std::views::transform(BPT_TL(_1.as_crs_dependency())));
    }

    auto meta_obj = json5::data::object_type();
    if (authors) {
        meta_obj.emplace("authors",
                         *authors                                              //
                             | std::views::transform(BPT_TL(json5::data(_1)))  //
                             | neo::to_vector);
    }
    if (description) {
        meta_obj.emplace("description", *description);
    }
    if (documentation) {
        meta_obj.emplace("documentation", documentation->normalized().to_string());
    }
    if (readme) {
        meta_obj.emplace("readme", readme->string());
    }
    if (homepage) {
        meta_obj.emplace("homepage", homepage->normalized().to_string());
    }
    if (repository) {
        meta_obj.emplace("repository", repository->normalized().to_string());
    }
    if (license) {
        meta_obj.emplace("license", license->to_string());
    }
    if (license_file) {
        meta_obj.emplace("license-file", license_file->string());
    }
    if (!meta_obj.empty()) {
        ret.meta = std::move(meta_obj);
    }
    return ret;
}
