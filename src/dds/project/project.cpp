#include "./project.hpp"

#include <dds/error/on_error.hpp>
#include <dds/error/result.hpp>
#include <dds/error/try_catch.hpp>
#include <dds/util/algo.hpp>
#include <dds/util/fs/io.hpp>
#include <dds/util/fs/path.hpp>
#include <dds/util/json5/convert.hpp>
#include <dds/util/json5/parse.hpp>

#include <neo/ranges.hpp>
#include <neo/tl.hpp>

using namespace dds;

project project::open_directory(path_ref path_) {
    DDS_E_SCOPE(e_open_project{path_});
    auto path = resolve_path_strong(path_).value();
    for (auto cand_fname : {"project.json", "project.jsonc", "project.json5", "pkg.json"}) {
        auto cand = path / cand_fname;
        if (!fs::exists(cand)) {
            continue;
        }
        DDS_E_SCOPE(e_parse_project_manifest_path{cand});
        auto data = parse_json5_file(cand);
        auto man  = project_manifest::from_json_data(data);
        return project{path, man};
    }
    return project{path, std::nullopt};
}

crs::package_meta project_manifest::as_crs_package_meta() const noexcept {
    crs::package_meta ret;
    ret.name         = name;
    ret.version      = version;
    ret.meta_version = 0;
    ret.namespace_   = namespace_.value_or(name);

    auto libs
        = libraries  //
        | std::views::transform([&](project_library lib) {
              auto deps = lib.lib_dependencies
                  | std::views::transform(NEO_TL(_1.as_crs_dependency())) | neo::to_vector;
              extend(deps,
                     root_dependencies | std::views::transform(NEO_TL(_1.as_crs_dependency())));
              return crs::library_meta{
                  .name         = lib.name,
                  .path         = lib.relpath,
                  .intra_uses   = lib.intra_uses.value_or(std::vector<crs::intra_usage>{}),
                  .dependencies = std::move(deps),
              };
          });
    ret.libraries = neo::to_vector(libs);
    if (ret.libraries.empty()) {
        ret.libraries.push_back(crs::library_meta{
            .name         = name,
            .path         = ".",
            .intra_uses   = {},
            .dependencies = {},
        });
    }
    return ret;
}
