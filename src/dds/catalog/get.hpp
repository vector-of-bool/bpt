#pragma once

#include <dds/source/dist.hpp>
#include <dds/temp.hpp>

namespace dds {

class pkg_cache;
class pkg_db;
struct package_info;

temporary_sdist get_package_sdist(const package_info&);

void get_all(const std::vector<package_id>& pkgs, dds::pkg_cache& repo, const pkg_db& cat);

}  // namespace dds
