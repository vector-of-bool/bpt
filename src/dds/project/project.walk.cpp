#include "./project.hpp"

#include "./error.hpp"

#include <dds/dym.hpp>
#include <dds/error/on_error.hpp>
#include <dds/error/result.hpp>
#include <dds/util/fs/op.hpp>
#include <dds/util/fs/path.hpp>
#include <dds/util/json_walk.hpp>
#include <dds/util/log.hpp>
#include <dds/util/url.hpp>

#include <fansi/styled.hpp>
#include <magic_enum.hpp>
#include <neo/tl.hpp>
#include <neo/ufmt.hpp>
#include <semester/walk.hpp>

#include <ranges>
#include <set>
#include <string>

using namespace dds;
using namespace dds::walk_utils;
using namespace fansi::literals;

namespace {

auto parse_root_lib = [](json5::data::mapping_type data) {
    if (data.find("path") != data.end()) {
        throw walk_error{"Project's main 'lib' cannot have a 'path' property"};
    }
    data.emplace("path", ".");
    return project_library::from_json_data(std::move(data));
};

auto path_key(std::string_view                      key,
              std::optional<std::filesystem::path>& into,
              const std::optional<fs::path>&        proj_dir) {
    return if_key{
        std::string(key),
        require_str{
            neo::ufmt("Project's '.bold.yellow[{}]' property must be a file path string"_styled,
                      key)},
        [key, &into, proj_dir](std::string const& s) {
            if (proj_dir) {
                std::filesystem::path abs = dds::resolve_path_weak(*proj_dir / s);
                if (!dds::file_exists(abs)) {
                    dds_log(
                        warn,
                        "Property '.blue[{}]' refers to non-existent path [.bold.yellow[{}]]"_styled,
                        key,
                        abs.string());
                }
            }
            into = normalize_path(s);
            return walk.accept;
        },
    };
}

auto url_key(std::string_view key, std::optional<neo::url>& into) {
    return if_key{
        std::string(key),
        require_str{
            neo::ufmt("Project's '.bold.yellow[{}]' property must be a URL string"_styled, key)},
        [key, &into](std::string const& s) {
            into = dds::parse_url(s);
            return walk.accept;
        },
    };
}

}  // namespace

project_manifest
project_manifest::from_json_data(const json5::data&                          data,
                                 const std::optional<std::filesystem::path>& proj_dir) {
    DDS_E_SCOPE(e_parse_project_manifest_data{data});
    project_manifest ret;

    key_dym_tracker dym{
        {
            "name",
            "version",
            "dependencies",
            "test-dependencies",
            "lib",
            "libs",
            "readme",
            "license-file",
            "homepage",
            "repository",
            "documentation",
            "description",
            "license",
            "authors",
        },
    };

    std::vector<std::string> authors;

    walk(data,
         require_mapping{"Project manifest root must be a mapping (JSON object)"},
         mapping{
             dym.tracker(),
             required_key{"name",
                          "A project 'name' is required",
                          require_str{"Project 'name' must be a string"},
                          put_into{ret.name, name_from_string{}}},
             required_key{"version",
                          "A project 'version' is required",
                          require_str{"Project 'version' must be a string"},
                          put_into{ret.version, version_from_string{}}},
             if_key{"dependencies",
                    require_array{"Project 'dependencies' should be an array"},
                    for_each{put_into{std::back_inserter(ret.root_dependencies),
                                      project_dependency::from_json_data}}},
             if_key{"test-dependencies",
                    require_array{"Project 'test-dependencies' should be an array"},
                    for_each{put_into{std::back_inserter(ret.root_test_dependencies),
                                      project_dependency::from_json_data}}},
             if_key{"lib",
                    require_mapping{"Project 'lib' must be a mapping (JSON object)"},
                    put_into(std::back_inserter(ret.libraries), parse_root_lib)},
             if_key{"libs",
                    for_each{put_into(std::back_inserter(ret.libraries),
                                      project_library::from_json_data)}},
             if_key{"$schema", just_accept},
             if_key{"x", just_accept},
             // Other package metadata
             path_key("readme", ret.readme, proj_dir),
             path_key("license-file", ret.license_file, proj_dir),
             url_key("homepage", ret.homepage),
             url_key("repository", ret.repository),
             url_key("documentation", ret.documentation),
             if_key{"description",
                    require_str{"Project 'description' must be a string"},
                    put_into(ret.description)},
             if_key{"license",
                    require_str("Project 'license' must be an SPDX license expression string"),
                    put_into(ret.license,
                             [](std::string s) { return sbs::spdx_license_expression::parse(s); })},
             if_key{"authors",
                    require_array{"Project 'authors' must be an array of strings"},
                    for_each{require_str{"Each element of project 'authors' must be a string"},
                             put_into(std::back_inserter(authors))}},
             dym.rejecter<e_bad_pkg_yaml_key>(),
         });

    if (dym.seen_keys.contains("authors")) {
        ret.authors = std::move(authors);
    }

    ret.as_crs_package_meta().throw_if_invalid();
    return ret;
}
