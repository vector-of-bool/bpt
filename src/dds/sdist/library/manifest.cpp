#include "./manifest.hpp"

#include <dds/dym.hpp>
#include <dds/error/errors.hpp>
#include <dds/util/algo.hpp>

#include <json5/parse_data.hpp>
#include <range/v3/view/transform.hpp>
#include <semester/decomp.hpp>

using namespace dds;

library_manifest library_manifest::load_from_file(path_ref fpath) {
    auto content = slurp_file(fpath);
    auto data    = json5::parse_data(content);

    if (!data.is_object()) {
        throw_user_error<errc::invalid_lib_manifest>("Root value must be an object");
    }

    library_manifest lib;
    using namespace semester::decompose_ops;
    auto res = semester::decompose(  //
        data,
        try_seq{require_type<json5::data::mapping_type>{
                    "The root of the library manifest must be an object (mapping)"},
                mapping{
                    if_key{"name",
                           require_type<std::string>{"`name` must be a string"},
                           put_into{lib.name}},
                    if_key{"uses",
                           require_type<json5::data::array_type>{
                               "`uses` must be an array of usage requirements"},
                           for_each{
                               require_type<std::string>{"`uses` elements must be strings"},
                               [&](auto&& uses) {
                                   lib.uses.push_back(lm::split_usage_string(uses.as_string()));
                                   return semester::dc_accept;
                               },
                           }},
                    if_key{"links",
                           require_type<json5::data::array_type>{
                               "`links` must be an array of usage requirements"},
                           for_each{
                               require_type<std::string>{"`links` elements must be strings"},
                               [&](auto&& links) {
                                   lib.links.push_back(lm::split_usage_string(links.as_string()));
                                   return semester::dc_accept;
                               },
                           }},
                }});
    auto rej = std::get_if<semester::dc_reject_t>(&res);
    if (rej) {
        throw_user_error<errc::invalid_lib_manifest>(rej->message);
    }

    if (lib.name.empty()) {
        throw_user_error<errc::invalid_lib_manifest>(
            "The 'name' field is required (Reading library manifest [{}])", fpath.string());
    }

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