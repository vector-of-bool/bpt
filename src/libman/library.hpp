#pragma once

#include <libman/util.hpp>

#include <optional>
#include <string>

namespace lm {

class library {
public:
    std::string              name;
    std::optional<fs::path>  linkable_path;
    std::vector<fs::path>    include_paths;
    std::vector<std::string> preproc_defs;
    std::vector<std::string> uses;
    std::vector<std::string> links;
    std::vector<std::string> special_uses;

    static library from_file(path_ref);
};

}  // namespace lm