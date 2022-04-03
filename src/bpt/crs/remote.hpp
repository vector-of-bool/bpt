#pragma once

#include "./info/pkg_id.hpp"

#include <bpt/util/fs/path.hpp>
#include <neo/url/view.hpp>

namespace dds::crs {

void pull_pkg_ar_from_remote(path_ref dest, neo::url_view from, pkg_id pkg);
void pull_pkg_from_remote(path_ref expand_into, neo::url_view from, pkg_id pkg);

}  // namespace dds::crs
