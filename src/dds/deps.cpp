#include "./deps.hpp"

#include <dds/repo/repo.hpp>
#include <dds/sdist.hpp>
#include <dds/usage_reqs.hpp>
#include <dds/util/string.hpp>
#include <libman/index.hpp>
#include <libman/parse.hpp>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

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

namespace {

fs::path generate_lml(const library_plan& lib, path_ref libdir, const build_env& env) {
    auto fname    = lib.name() + ".lml";
    auto lml_path = libdir / fname;

    std::vector<lm::pair> kvs;
    kvs.emplace_back("Type", "Library");
    kvs.emplace_back("Name", lib.name());
    if (lib.create_archive()) {
        kvs.emplace_back("Path",
                         fs::relative(lib.create_archive()->calc_archive_file_path(env),
                                      lml_path.parent_path())
                             .string());
    }
    auto pub_inc_dir = lib.source_root() / "include";
    auto src_dir     = lib.source_root() / "src";
    if (!fs::exists(pub_inc_dir)) {
        pub_inc_dir = src_dir;
    }
    kvs.emplace_back("Include-Path", pub_inc_dir.string());

    // TODO: Uses, Preprocessor-Define, and Special-Uses

    fs::create_directories(lml_path.parent_path());
    lm::write_pairs(lml_path, kvs);
    return lml_path;
}

fs::path generate_lmp(const package_plan& pkg, path_ref basedir, const build_env& env) {
    auto fname    = pkg.name() + ".lmp";
    auto lmp_path = basedir / fname;

    std::vector<lm::pair> kvs;
    kvs.emplace_back("Type", "Package");
    kvs.emplace_back("Name", pkg.name());
    kvs.emplace_back("Namespace", pkg.namespace_());

    for (auto&& lib : pkg.libraries()) {
        auto lml = generate_lml(lib, basedir / pkg.name(), env);
        kvs.emplace_back("Library", fs::relative(lml, lmp_path.parent_path()).string());
    }

    // TODO: `Requires` for transitive package imports

    fs::create_directories(lmp_path.parent_path());
    lm::write_pairs(lmp_path, kvs);
    return lmp_path;
}

}  // namespace

void dds::write_libman_index(path_ref out_filepath, const build_plan& plan, const build_env& env) {
    fs::create_directories(out_filepath.parent_path());
    auto                  lm_items_dir = out_filepath.parent_path() / "_libman";
    std::vector<lm::pair> kvs;
    kvs.emplace_back("Type", "Index");
    for (const package_plan& pkg : plan.packages()) {
        auto pkg_lmp = generate_lmp(pkg, lm_items_dir, env);
        kvs.emplace_back("Package",
                         fmt::format("{}; {}",
                                     pkg.name(),
                                     fs::relative(pkg_lmp, out_filepath.parent_path()).string()));
    }
    lm::write_pairs(out_filepath, kvs);
}