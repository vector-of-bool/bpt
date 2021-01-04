#pragma once

#include <dds/deps.hpp>
#include <dds/pkg/id.hpp>

#include <neo/url.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace dds {

class remote_pkg_base;

class any_remote_pkg {
    std::shared_ptr<const remote_pkg_base> _impl;

    explicit any_remote_pkg(std::shared_ptr<const remote_pkg_base> p)
        : _impl(p) {}

public:
    any_remote_pkg();
    ~any_remote_pkg();

    static any_remote_pkg from_url(const neo::url& url);

    neo::url    to_url() const;
    std::string to_url_string() const;
    void        get_sdist(path_ref dest) const;
};

struct pkg_listing {
    pkg_id                  ident;
    std::vector<dependency> deps{};
    std::string             description{};

    any_remote_pkg remote_pkg{};
};

}  // namespace dds
