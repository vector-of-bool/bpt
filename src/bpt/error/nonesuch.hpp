#pragma once

#include <optional>
#include <string>

#include <iosfwd>

namespace bpt {

struct e_nonesuch {
    std::string                given;
    std::optional<std::string> nearest;

    e_nonesuch(std::string_view gn, std::optional<std::string> nr) noexcept
        : given{gn}
        , nearest{nr} {}

    void log_error(std::string_view fmt) const noexcept;

    void ostream_into(std::ostream& out) const noexcept;

    friend std::ostream& operator<<(std::ostream& out, const e_nonesuch& self) noexcept {
        self.ostream_into(out);
        return out;
    }
};

}  // namespace bpt
