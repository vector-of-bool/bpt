#pragma once

#include <dds/error/result_fwd.hpp>
#include <libman/util.hpp>

#include <optional>
#include <string>
#include <string_view>

#include <fmt/core.h>

namespace lm {

struct usage {
    std::string namespace_;
    std::string name;

    auto operator<=>(const usage&) const noexcept = default;
};

struct e_invalid_usage_string {
    std::string value;
};

dds::result<usage> split_usage_string(std::string_view);

class library {
public:
    std::string              name;
    std::optional<fs::path>  linkable_path{};
    std::vector<fs::path>    include_paths{};
    std::vector<std::string> preproc_defs{};
    std::vector<usage>       uses{};
    std::vector<usage>       links{};
    std::vector<std::string> special_uses{};

    static dds::result<library> from_file(path_ref);
};

}  // namespace lm

namespace fmt {
template <>
struct formatter<lm::usage> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const lm::usage& usage, FormatContext& ctx) {
        return format_to(ctx.out(), "'{}/{}'", usage.namespace_, usage.name);
    }
};
}  // namespace fmt