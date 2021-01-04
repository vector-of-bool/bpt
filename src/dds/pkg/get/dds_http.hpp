#pragma once

#include "./base.hpp"

#include <dds/pkg/id.hpp>

#include <neo/url.hpp>

#include <string>
#include <string_view>

namespace dds {

class dds_http_remote_pkg : public remote_pkg_base {
    void     do_get_raw(path_ref) const override;
    neo::url do_to_url() const override;

public:
    neo::url    repo_url;
    dds::pkg_id pkg_id;

    dds_http_remote_pkg() = default;

    dds_http_remote_pkg(neo::url u, dds::pkg_id pid)
        : repo_url(u)
        , pkg_id(pid) {}

    static dds_http_remote_pkg from_url(const neo::url& url);
};

}  // namespace dds
