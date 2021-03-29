#include "./manifest.hpp"

#include <dds/dym.hpp>
#include <dds/error/errors.hpp>
#include <dds/error/on_error.hpp>
#include <dds/error/result.hpp>
#include <dds/util/algo.hpp>

#include <json5/parse_data.hpp>
#include <semester/walk.hpp>

using namespace dds;

using require_obj   = semester::require_type<json5::data::mapping_type>;
using require_array = semester::require_type<json5::data::array_type>;
using require_str   = semester::require_type<std::string>;

library_manifest library_manifest::load_from_file(path_ref fpath) {
    DDS_E_SCOPE(e_library_manifest_path{fpath.string()});

    auto content = slurp_file(fpath);
    auto data    = json5::parse_data(content);

    if (!data.is_object()) {
        throw_user_error<errc::invalid_lib_manifest>("Root value must be an object");
    }

    library_manifest lib;
    using namespace semester::walk_ops;
    // Helpers
    auto str_to_usage = [](const std::string& s) { return lm::split_usage_string(s); };
    auto append_usage
        = [&](auto& usages) { return put_into(std::back_inserter(usages), str_to_usage); };

    // Parse and validate
    walk(data,
         require_obj{"Root of library manifest should be a JSON object"},
         mapping{
             if_key{"$schema", just_accept},
             required_key{"name",
                          "A string 'name' is required",
                          require_str{"'name' must be a string"},
                          put_into{lib.name,
                                   [](std::string s) { return *dds::name::from_string(s); }}},
             if_key{"uses",
                    require_array{"'uses' must be an array of strings"},
                    for_each{require_str{"Each 'uses' element should be a string"},
                             append_usage(lib.uses)}},
             if_key{"test_uses",
                    require_array{"'test_uses' must be an array of strings"},
                    for_each{require_str{"Each 'test_uses' element should be a string"},
                             append_usage(lib.test_uses)}},
         });

    return lib;
}

std::optional<fs::path> library_manifest::find_in_directory(path_ref dirpath) {
    auto fnames = {
        "library.json5",
        "library.jsonc",
        "library.json",
    };
    for (auto c : fnames) {
        auto cand = dirpath / c;
        if (fs::is_regular_file(cand)) {
            return cand;
        }
    }

    return std::nullopt;
}

std::optional<library_manifest> library_manifest::load_from_directory(path_ref dirpath) {
    auto found = find_in_directory(dirpath);
    if (!found.has_value()) {
        return std::nullopt;
    }

    return load_from_file(*found);
}
