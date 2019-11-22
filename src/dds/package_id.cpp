#include <dds/package_id.hpp>

#include <spdlog/fmt/fmt.h>

#include <tuple>

using namespace dds;

package_id package_id::parse(std::string_view s) {
    auto at_pos = s.find('@');
    if (at_pos == s.npos) {
        throw std::runtime_error(fmt::format("Invalid package ID string '{}'", s));
    }

    auto name    = s.substr(0, at_pos);
    auto ver_str = s.substr(at_pos + 1);

    return {std::string(name), semver::version::parse(ver_str)};
}

std::string package_id::to_string() const noexcept { return name + "@" + version.to_string(); }