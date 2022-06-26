#pragma once

#include "./usage.hpp"
#include <bpt/error/result_fwd.hpp>
#include <libman/util.hpp>

#include <optional>
#include <string>
#include <string_view>

#include <fmt/core.h>

namespace lm {

class library {
public:
    std::string              name;
    std::optional<fs::path>  linkable_path{};
    std::vector<fs::path>    include_paths{};
    std::vector<std::string> preproc_defs{};
    std::vector<usage>       uses{};
    std::vector<usage>       links{};
    std::vector<std::string> special_uses{};

    static bpt::result<library> from_file(path_ref);
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