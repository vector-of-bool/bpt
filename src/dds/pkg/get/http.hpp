#pragma once

#include "./base.hpp"

#include <neo/url.hpp>

#include <string>
#include <string_view>

namespace dds {

class http_remote_pkg : public remote_pkg_base {
    void     do_get_raw(path_ref) const override;
    neo::url do_to_url() const override;

public:
    neo::url url;
    unsigned strip_n_components = 0;

    http_remote_pkg() = default;

    http_remote_pkg(neo::url u, unsigned strpcmp)
        : url(u)
        , strip_n_components(strpcmp) {}

    static http_remote_pkg from_url(const neo::url& url);
};

}  // namespace dds
