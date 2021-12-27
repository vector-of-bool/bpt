#include "./dependency.hpp"

#include <sstream>

namespace {

std::string iv_string(const pubgrub::interval_set<semver::version>::interval_type& iv) {
    if (iv.high == semver::version::max_version()) {
        return ">=" + iv.low.to_string();
    }
    if (iv.low == semver::version()) {
        return "<" + iv.high.to_string();
    }
    return iv.low.to_string() + " < " + iv.high.to_string();
}

semver::version next_major(semver::version const& v) {
    auto v2  = v;
    v2.minor = 0;
    v2.patch = 0;
    ++v2.major;
    v2.prerelease = {};
    return v2;
}

semver::version next_minor(semver::version const& v) {
    auto v2  = v;
    v2.patch = 0;
    ++v2.minor;
    v2.prerelease = {};
    return v2;
}

}  // namespace

std::string dds::crs::dependency::decl_to_string() const noexcept {
    std::stringstream strm;
    strm << name.str;
    if (acceptable_versions.num_intervals() == 1) {
        auto iv = *acceptable_versions.iter_intervals().begin();
        if (iv.high == iv.low.next_after()) {
            strm << "@" << iv.low.to_string();
            return strm.str();
        }
        if (iv.high == next_major(iv.low)) {
            strm << "^" << iv.low.to_string();
            return strm.str();
        }
        if (iv.high == next_minor(iv.low)) {
            strm << "~" << iv.low.to_string();
            return strm.str();
        }
        strm << "@";
        if (iv.low == semver::version() && iv.high == semver::version::max_version()) {
            return name.str;
        }
        strm << "@[" << iv_string(iv) << "]";
        return strm.str();
    }

    strm << "@[";
    auto       iv_it = acceptable_versions.iter_intervals();
    auto       it    = iv_it.begin();
    const auto stop  = iv_it.end();
    while (it != stop) {
        strm << "(" << iv_string(*it) << ")";
        ++it;
        if (it != stop) {
            strm << " || ";
        }
    }
    strm << "]";
    return strm.str();
}