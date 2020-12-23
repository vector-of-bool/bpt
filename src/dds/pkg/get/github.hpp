#pragma once

#include "./base.hpp"

#include <neo/url.hpp>

#include <string>
#include <string_view>

namespace dds {

class github_remote_pkg : public remote_pkg_base {
    void     do_get_raw(path_ref) const override;
    neo::url do_to_url() const override;

public:
    std::string owner;
    std::string reponame;
    std::string ref;

    static github_remote_pkg from_url(const neo::url&);
};

}  // namespace dds
