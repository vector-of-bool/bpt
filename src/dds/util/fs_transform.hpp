#pragma once

#include "./fs.hpp"
#include "./glob.hpp"

#include <json5/data.hpp>

#include <optional>
#include <variant>

namespace dds {

struct fs_transformation {
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

    struct write {
        fs::path    path;
        std::string content;
    };

    std::optional<struct copy>  copy;
    std::optional<struct move>  move;
    std::optional<remove>       remove;
    std::optional<struct write> write;

    void apply_to(path_ref root) const;

    static fs_transformation from_json(const json5::data&);

    std::string as_json() const noexcept;
};

}  // namespace dds
