#pragma once

#include <optional>
#include <string>

namespace dds {

struct e_nonesuch {
    std::string                given;
    std::optional<std::string> nearest;

    e_nonesuch(std::string_view gn, std::optional<std::string> nr) noexcept
        : given{gn}
        , nearest{nr} {}

    void log_error(std::string_view fmt) const noexcept;
};

}  // namespace dds
