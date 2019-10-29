#include "./library.hpp"

#include <libman/parse.hpp>

#include <spdlog/spdlog.h>

using namespace lm;

library library::from_file(path_ref fpath) {
    auto pairs = parse_file(fpath);

    library ret;

    std::vector<std::string> uses_strs;
    std::vector<std::string> links_strs;

    std::string _type_;
    read(fmt::format("Reading library manifest file '{}'", fpath.string()),
         pairs,
         read_required("Type", _type_),
         read_check_eq("Type", "Library"),
         read_required("Name", ret.name),
         read_opt("Path", ret.linkable_path),
         read_accumulate("Include-Path", ret.include_paths),
         read_accumulate("Preprocessor-Define", ret.preproc_defs),
         read_accumulate("Uses", uses_strs),
         read_accumulate("Links", links_strs),
         read_accumulate("Special-Uses", ret.special_uses));

    for (auto&& uses_str : uses_strs) {
        ret.uses.push_back(split_usage_string(uses_str));
    }

    for (auto&& links_str : links_strs) {
        ret.links.push_back(split_usage_string(links_str));
    }

    auto make_absolute = [&](path_ref p) { return fpath.parent_path() / p; };
    std::transform(ret.include_paths.begin(),
                   ret.include_paths.end(),
                   ret.include_paths.begin(),
                   make_absolute);

    if (ret.linkable_path) {
        ret.linkable_path = make_absolute(*ret.linkable_path);
    }

    return ret;
}

usage lm::split_usage_string(std::string_view str) {
    auto sl_pos = str.find('/');
    if (sl_pos == str.npos) {
        throw std::runtime_error("Invalid Uses/Links specifier: " + std::string(str));
    }
    auto ns   = str.substr(0, sl_pos);
    auto name = str.substr(sl_pos + 1);
    return usage{std::string(ns), std::string(name)};
}