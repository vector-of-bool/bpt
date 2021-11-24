#include <dds/pkg/id.hpp>

#include <dds/error/errors.hpp>
#include <dds/util/result.hpp>

#include <fmt/core.h>

#include <tuple>

using namespace dds;

pkg_id pkg_id::parse(const std::string_view s) {
    DDS_E_SCOPE(e_pkg_id_str{std::string(s)});
    auto at_pos = s.find('@');
    if (at_pos == s.npos) {
        BOOST_LEAF_THROW_EXCEPTION(
            make_user_error<errc::invalid_pkg_id>("Package ID must contain an '@' symbol"));
    }

    auto name    = dds::name::from_string(s.substr(0, at_pos)).value();
    auto ver_str = s.substr(at_pos + 1);

    try {
        return {name, semver::version::parse(ver_str)};
    } catch (const semver::invalid_version& err) {
        BOOST_LEAF_THROW_EXCEPTION(make_user_error<errc::invalid_pkg_id>(), err);
    }
}

std::string pkg_id::to_string() const noexcept {
    return fmt::format("{}@{}", name.str, version.to_string());
}
