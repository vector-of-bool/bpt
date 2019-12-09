#include "./deps.hpp"

#include <dds/repo/repo.hpp>
#include <dds/sdist.hpp>
#include <dds/usage_reqs.hpp>
#include <dds/util/string.hpp>
#include <libman/index.hpp>
#include <libman/parse.hpp>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <cctype>
#include <map>
#include <set>

using namespace dds;

dependency dependency::parse_depends_string(std::string_view str) {
    const auto str_begin = str.data();
    auto       str_iter  = str_begin;
    const auto str_end   = str_iter + str.size();

    while (str_iter != str_end && !std::isspace(*str_iter)) {
        ++str_iter;
    }

    auto name        = trim_view(std::string_view(str_begin, str_iter - str_begin));
    auto version_str = trim_view(std::string_view(str_iter, str_end - str_iter));

    try {
        auto rng = semver::range::parse_restricted(version_str);
        return dependency{std::string(name), {rng.low(), rng.high()}};
    } catch (const semver::invalid_range&) {
        throw std::runtime_error(fmt::format(
            "Invalid version range string '{}' in dependency declaration '{}' (Should be a "
            "semver range string. See https://semver.org/ for info)",
            version_str,
            str));
    }
}
