#include "./pkg_id.hpp"

#include <boost/leaf/exception.hpp>
#include <dds/error/human.hpp>
#include <dds/error/on_error.hpp>
#include <dds/error/result.hpp>
#include <neo/ufmt.hpp>

#include <charconv>
#include <tuple>

using namespace dds;
using namespace dds::crs;

crs::pkg_id crs::pkg_id::parse(const std::string_view sv) {
    DDS_E_SCOPE(e_invalid_pkg_id_str{std::string(sv)});
    auto at_pos = sv.find("@");
    if (at_pos == sv.npos) {
        BOOST_LEAF_THROW_EXCEPTION(e_human_message{
            "Package ID must contain an '@' character between the package name and the version"});
    }

    auto name_sv = sv.substr(0, at_pos);
    auto name    = *dds::name::from_string(name_sv);

    auto ver_str = sv.substr(at_pos + 1);

    auto tilde_pos   = ver_str.find("~");
    int  pkg_version = 0;
    if (tilde_pos != ver_str.npos) {
        auto tail   = ver_str.substr(tilde_pos + 1);
        auto fc_res = std::from_chars(tail.data(), tail.data() + tail.size(), pkg_version);
        if (fc_res.ec != std::errc{}) {
            BOOST_LEAF_THROW_EXCEPTION(
                e_human_message{"Invalid pkg-version integer suffix in package ID string"});
        }
        ver_str = ver_str.substr(0, tilde_pos);
    }
    auto version = semver::version::parse(ver_str);
    return crs::pkg_id{name, version, pkg_version};
}

std::string crs::pkg_id::to_string() const noexcept {
    return neo::ufmt("{}@{}~{}", name.str, version.to_string(), revision);
}

static auto _tie(const crs::pkg_id& pid) noexcept {
    return std::tie(pid.name, pid.version, pid.revision);
}

bool crs::pkg_id::operator<(const crs::pkg_id& other) const noexcept {
    return _tie(*this) < _tie(other);
}
