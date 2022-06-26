#include "./json_walk.hpp"

#include <bpt/error/result.hpp>
#include <bpt/util/name.hpp>

#include <semver/version.hpp>

bpt::name bpt::walk_utils::name_from_string::operator()(std::string s) {
    return bpt::name::from_string(s).value();
}

semver::version bpt::walk_utils::version_from_string::operator()(std::string s) {
    return semver::version::parse(s);
}
