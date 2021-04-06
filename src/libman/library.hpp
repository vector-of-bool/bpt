#pragma once

#include <dds/error/result_fwd.hpp>
#include <libman/util.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace lm {

struct usage {
    std::string namespace_;
    std::string name;
};

struct e_invalid_usage_string {
    std::string value;
};

dds::result<usage> split_usage_string(std::string_view);

class library {
public:
    std::string              name;
    std::optional<fs::path>  linkable_path;
    std::vector<fs::path>    include_paths;
    std::vector<std::string> preproc_defs;
    std::vector<usage>       uses;
    std::vector<usage>       links;
    std::vector<std::string> special_uses;

    static dds::result<library> from_file(path_ref);
};

}  // namespace lm