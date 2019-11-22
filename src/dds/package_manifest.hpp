#pragma once

#include <dds/deps.hpp>
#include <dds/package_id.hpp>
#include <dds/util/fs.hpp>
#include <semver/version.hpp>

#include <optional>
#include <string>
#include <vector>

namespace dds {

enum class test_lib {
    catch_,
    catch_main,
    catch_runner,
};

struct package_manifest {
    package_id              pk_id;
    std::string             namespace_;
    std::optional<test_lib> test_driver;
    std::vector<dependency> dependencies;
    static package_manifest load_from_file(path_ref);
};

}  // namespace dds