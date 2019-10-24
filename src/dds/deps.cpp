#include "./deps.hpp"

#include <dds/sdist.hpp>
#include <dds/repo/repo.hpp>
#include <dds/util/string.hpp>

#include <range/v3/algorithm/partition_point.hpp>
#include <spdlog/fmt/fmt.h>

#include <cctype>

using namespace dds;

dependency dependency::parse_depends_string(std::string_view str) {
    const auto str_begin = str.data();
    auto       str_iter  = str_begin;
    const auto str_end   = str_iter + str.size();

    while (str_iter != str_end && !std::isspace(*str_iter)) {
        ++str_iter;
    }

    auto name        = trim(std::string_view(str_begin, str_iter - str_begin));
    auto version_str = trim(std::string_view(str_iter, str_end - str_iter));

    semver::version version;
    try {
        version = semver::version::parse(version_str);
    } catch (const semver::invalid_version&) {
        throw std::runtime_error(
            fmt::format("Invalid version string '{}' in dependency declaration '{}' (Should be a "
                        "semver string. See https://semver.org/ for info)",
                        version_str,
                        str));
    }
    return dependency{std::string(name), version};
}

std::vector<sdist> dds::find_dependencies(const repository& repo, const dependency& dep) {
    std::vector<sdist> acc;
    detail::do_find_deps(repo, dep, acc);
    return acc;
}

void detail::do_find_deps(const repository& repo, const dependency& dep, std::vector<sdist>& sd) {
    auto sdist_opt = repo.get_sdist(dep.name, dep.version.to_string());
    if (!sdist_opt) {
        throw std::runtime_error(
            fmt::format("Unable to find dependency to satisfy requirement: {} {}",
                        dep.name,
                        dep.version.to_string()));
    }
    sdist& new_sd = *sdist_opt;
    auto insert_point = ranges::partition_point(sd, [&](const sdist& cand) {
        return cand.path < new_sd.path;
    });
    if (insert_point != sd.end() && insert_point->manifest.name == new_sd.manifest.name) {
        if (insert_point->manifest.version != new_sd.manifest.version) {
            assert(false && "Version conflict resolution not implemented yet");
            std::terminate();
        }
        return;
    }
    sd.insert(insert_point, std::move(new_sd));
}