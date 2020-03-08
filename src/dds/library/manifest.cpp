#include "./manifest.hpp"

#include <dds/dym.hpp>
#include <dds/error/errors.hpp>
#include <dds/util/algo.hpp>
#include <libman/parse.hpp>

#include <json5/parse_data.hpp>
#include <range/v3/view/transform.hpp>
#include <semester/decomp.hpp>
#include <spdlog/spdlog.h>

using namespace dds;

library_manifest library_manifest::load_from_dds_file(path_ref fpath) {
    spdlog::warn(
        "Using deprecated library.dds parsing (on file {}). This will be removed soon. Migrate!",
        fpath.string());
    auto             kvs = lm::parse_file(fpath);
    library_manifest ret;
    ret.name = fpath.parent_path().filename().string();
    std::vector<std::string> uses_strings;
    std::vector<std::string> links_strings;
    lm::read(fmt::format("Reading library manifest {}", fpath.string()),
             kvs,
             lm::read_accumulate("Uses", uses_strings),
             lm::read_accumulate("Links", links_strings),
             lm::read_required("Name", ret.name),
             lm_reject_dym{{"Uses", "Links", "Name"}});

    extend(ret.uses, ranges::views::transform(uses_strings, lm::split_usage_string));
    extend(ret.links, ranges::views::transform(links_strings, lm::split_usage_string));
    return ret;
}

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
    // using namespace json_read::ops;
    // json_read::decompose(  //
    //     data.as_object(),
    //     object(key("name", require_string(put_into{lib.name}, "`name` must be a string")),
    //            key("uses",
    //                array_each{require_string(
    //                    [&](auto&& uses) {
    //                        lib.uses.push_back(lm::split_usage_string(uses.as_string()));
    //                        return json_read::accept_t{};
    //                    },
    //                    "All `uses` items must be strings")}),
    //            key("links",
    //                array_each{require_string(
    //                    [&](auto&& links) {
    //                        lib.links.push_back(lm::split_usage_string(links.as_string()));
    //                        return json_read::accept_t{};
    //                    },
    //                    "All `links` items must be strings")})));

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

    auto dds_file = dirpath / "library.dds";
    if (fs::is_regular_file(dds_file)) {
        return dds_file;
    }

    return std::nullopt;
}

std::optional<library_manifest> library_manifest::load_from_directory(path_ref dirpath) {
    auto found = find_in_directory(dirpath);
    if (!found.has_value()) {
        return std::nullopt;
    }

    if (found->extension() == ".dds") {
        return load_from_dds_file(*found);
    } else {
        return load_from_file(*found);
    }
}