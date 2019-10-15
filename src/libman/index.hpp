#pragma once

#include <libman/package.hpp>
#include <libman/util.hpp>

#include <functional>
#include <map>

namespace lm {

class index {
public:
    std::vector<package> packages;
    static index         from_file(path_ref);

    using library_index
        = std::map<std::pair<std::string, std::string>, std::reference_wrapper<const library>>;
    library_index build_library_index() const;
};

}  // namespace lm