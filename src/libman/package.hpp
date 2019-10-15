#pragma once

#include <libman/library.hpp>
#include <libman/util.hpp>

namespace lm {

class package {
public:
    std::string name;
    std::string namespace_;
    std::vector<std::string> requires;
    std::vector<library> libraries;
    fs::path             lmp_path;

    static package       from_file(path_ref);
};

}  // namespace lm