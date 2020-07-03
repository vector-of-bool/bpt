#pragma once

#include "./fs.hpp"
#include "./glob.hpp"

#include <optional>
#include <variant>

namespace dds {

class fs_transformation {
    struct copy_move_base {
        fs::path from;
        fs::path to;

        int                    strip_components = 0;
        std::vector<dds::glob> include;
        std::vector<dds::glob> exclude;
    };

    struct copy : copy_move_base {};
    struct move : copy_move_base {};

    struct remove {
        fs::path path;

        std::vector<dds::glob> only_matching;
    };

    std::optional<struct copy> copy;
    std::optional<struct move> move;
    std::optional<remove>      remove;

    void apply_to(path_ref root) const;
};

}  // namespace dds
