#pragma once

#include <dds/source/dist.hpp>
#include <dds/temp.hpp>

namespace dds {

class repository;
class catalog;
struct package_info;

temporary_sdist get_package_sdist(const package_info&);

void get_all(const std::vector<package_id>& pkgs, dds::repository& repo, const catalog& cat);

}  // namespace dds
