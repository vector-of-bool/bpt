#ifndef DDS_MANIFEST_HPP_INCLUDED
#define DDS_MANIFEST_HPP_INCLUDED

#include <dds/util.hpp>

namespace dds {

struct library_manifest {
    std::vector<fs::path>    private_includes;
    std::vector<std::string> private_defines;

    static library_manifest load_from_file(const fs::path&);
};

}  // namespace dds

#endif  // DDS_MANIFEST_HPP_INCLUDED