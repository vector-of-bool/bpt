#include "./json_walk.hpp"

#include <dds/error/result.hpp>

#include <dds/pkg/name.hpp>
#include <semver/version.hpp>

dds::name dds::walk_utils::name_from_string::operator()(std::string s) {
    return dds::name::from_string(s).value();
}

semver::version dds::walk_utils::version_from_string::operator()(std::string s) {
    return semver::version::parse(s);
}
