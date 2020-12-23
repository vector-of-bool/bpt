#pragma once

#include <libman/package.hpp>
#include <neo/concepts.hpp>
#include <neo/url.hpp>

#include <optional>
#include <vector>

namespace dds {

struct pkg_id;

class remote_pkg_base {
    virtual void     do_get_raw(path_ref dest) const = 0;
    virtual neo::url do_to_url() const               = 0;

public:
    void get_sdist(path_ref dest) const;
    void get_raw_directory(path_ref dest) const;

    neo::url    to_url() const;
    std::string to_url_string() const;
};

}  // namespace dds
