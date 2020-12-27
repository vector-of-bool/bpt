#pragma once

#include "./base.hpp"

#include <neo/url.hpp>

#include <string>

namespace dds {

class git_remote_pkg : public remote_pkg_base {
    void     do_get_raw(path_ref) const override;
    neo::url do_to_url() const override;

public:
    neo::url    url;
    std::string ref;

    static git_remote_pkg from_url(const neo::url&);
};

}  // namespace dds
