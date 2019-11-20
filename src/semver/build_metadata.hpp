#pragma once

#include <semver/ident.hpp>

namespace semver {

class build_metadata {
    std::vector<ident> _ids;

public:
    build_metadata() = default;

    [[nodiscard]] bool empty() const noexcept { return _ids.empty(); }

    void add_ident(ident id) noexcept { _ids.push_back(id); }
    void add_ident(std::string_view s) { add_ident(ident(s)); }

    auto& idents() const noexcept { return _ids; }

    static build_metadata parse(std::string_view s) {
        build_metadata ret;
        ret._ids = ident::parse_dotted_seq(s);
        return ret;
    }
};

}  // namespace semver