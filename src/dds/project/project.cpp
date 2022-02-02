#include "./project.hpp"

#include "./error.hpp"

#include <dds/error/on_error.hpp>
#include <dds/error/result.hpp>
#include <dds/error/try_catch.hpp>
#include <dds/util/algo.hpp>
#include <dds/util/fs/io.hpp>
#include <dds/util/fs/op.hpp>
#include <dds/util/fs/path.hpp>
#include <dds/util/json5/convert.hpp>
#include <dds/util/json5/parse.hpp>
#include <dds/util/log.hpp>
#include <dds/util/yaml/convert.hpp>
#include <dds/util/yaml/parse.hpp>

#include <neo/ranges.hpp>
#include <neo/tl.hpp>

using namespace dds;

project project::open_directory(path_ref path_) {
    DDS_E_SCOPE(e_open_project{path_});
    auto path     = resolve_path_strong(path_).value();
    auto pkg_yaml = path / "pkg.yaml";
    if (!dds::file_exists(pkg_yaml)) {
        if (dds::file_exists(path / "pkg.yml")) {
            dds_log(warn,
                    "There's a [pkg.yml] file in the project directory, but dds expects a '.yaml' "
                    "file extension. The file [{}] will be ignored.",
                    (path / "pkg.yml").string());
        }
        return project{path, std::nullopt};
    }
    DDS_E_SCOPE(e_parse_project_manifest_path{pkg_yaml});
    auto yml  = sbs::parse_yaml_file(pkg_yaml);
    auto data = sbs::yaml_as_json5_data(yml);
    auto man  = project_manifest::from_json_data(data, path);
    return project{path, man};
}

crs::package_info project_manifest::as_crs_package_meta() const noexcept {
    crs::package_info ret;
    ret.id.name         = name;
    ret.id.version      = version;
    ret.id.pkg_revision = 1;

    auto libs = libraries  //
        | std::views::transform([&](project_library lib) {
                    auto deps = lib.lib_dependencies
                        | std::views::transform(&project_dependency::as_crs_dependency)
                        | neo::to_vector;
                    extend(deps,
                           root_dependencies
                               | std::views::transform(&project_dependency::as_crs_dependency));
                    return crs::library_info{
                        .name         = lib.name,
                        .path         = lib.relpath,
                        .intra_uses   = lib.intra_uses.value_or(std::vector<crs::intra_usage>{}),
                        .dependencies = std::move(deps),
                    };
                });
    ret.libraries = neo::to_vector(libs);
    if (ret.libraries.empty()) {
        ret.libraries.push_back(crs::library_info{
            .name         = name,
            .path         = ".",
            .intra_uses   = {},
            .dependencies = {},
        });
        extend(ret.libraries.back().dependencies,
               root_dependencies | std::views::transform(NEO_TL(_1.as_crs_dependency())));
    }
    return ret;
}
