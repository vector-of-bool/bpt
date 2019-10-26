#include "./deps.hpp"

#include <dds/repo/repo.hpp>
#include <dds/sdist.hpp>
#include <dds/util/string.hpp>
#include <libman/index.hpp>
#include <libman/parse.hpp>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include <cctype>
#include <map>

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
    for (const auto& inner_dep : new_sd.manifest.dependencies) {
        do_find_deps(repo, inner_dep, sd);
    }
    auto insert_point = std::partition_point(sd.begin(), sd.end(), [&](const sdist& cand) {
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

using sdist_index_type = std::map<std::string, std::reference_wrapper<const sdist>>;

namespace {

void linkup_dependencies(shared_compile_file_rules& rules,
                         const package_manifest&    man,
                         const sdist_index_type&    sd_idx) {
    for (const dependency& dep : man.dependencies) {
        auto found = sd_idx.find(dep.name);
        if (found == sd_idx.end()) {
            throw std::runtime_error(
                fmt::format("Unable to resolve dependency '{}' (required by '{}')",
                            dep.name,
                            man.name));
        }
        linkup_dependencies(rules, found->second.get().manifest, sd_idx);
        auto lib_src     = found->second.get().path / "src";
        auto lib_include = found->second.get().path / "include";
        if (fs::exists(lib_include)) {
            rules.include_dirs().push_back(lib_include);
        } else {
            rules.include_dirs().push_back(lib_src);
        }
    }
}

void add_sdist_to_dep_plan(build_plan& plan, const sdist& sd, const sdist_index_type& sd_idx) {
    auto& pkg = plan.build_packages.emplace_back();
    pkg.name  = sd.manifest.name;
    auto libs = collect_libraries(sd.path, sd.manifest.name);
    for (const auto& lib : libs) {
        shared_compile_file_rules comp_rules = lib.base_compile_rules();
        linkup_dependencies(comp_rules, sd.manifest, sd_idx);
        library_build_params params;
        params.compile_rules = comp_rules;
        pkg.add_library(lib, params);
    }
}

}  // namespace

build_plan dds::create_deps_build_plan(const std::vector<sdist>& deps) {
    auto sd_idx = deps | ranges::views::transform([](const auto& sd) {
                      return std::pair(sd.manifest.name, std::cref(sd));
                  })
        | ranges::to<sdist_index_type>();

    build_plan plan;
    for (const sdist& sd : deps) {
        spdlog::info("Recording dependency: {}", sd.manifest.name);
        add_sdist_to_dep_plan(plan, sd, sd_idx);
    }
    return plan;
}

namespace {

fs::path generate_lml(const library_plan& lib, path_ref libdir, const build_env& env) {
    auto fname    = lib.name + ".lml";
    auto lml_path = libdir / fname;

    std::vector<lm::pair> kvs;
    kvs.emplace_back("Type", "Library");
    kvs.emplace_back("Name", lib.name);
    if (lib.create_archive) {
        kvs.emplace_back("Path",
                         fs::relative(lib.create_archive->archive_file_path(env),
                                      lml_path.parent_path()).string());
    }
    auto pub_inc_dir = lib.source_root / "include";
    auto src_dir = lib.source_root / "src";
    if (fs::exists(src_dir)) {
        pub_inc_dir = src_dir;
    }
    kvs.emplace_back("Include-Path", pub_inc_dir.string());

    // TODO: Uses, Preprocessor-Define, and Special-Uses

    fs::create_directories(lml_path.parent_path());
    lm::write_pairs(lml_path, kvs);
    return lml_path;
}

fs::path generate_lmp(const package_plan& pkg, path_ref basedir, const build_env& env) {
    auto fname    = pkg.name + ".lmp";
    auto lmp_path = basedir / fname;

    std::vector<lm::pair> kvs;
    kvs.emplace_back("Type", "Package");
    kvs.emplace_back("Name", pkg.name);
    kvs.emplace_back("Namespace", pkg.name);

    for (auto&& lib : pkg.create_libraries) {
        auto lml = generate_lml(lib, basedir / pkg.name, env);
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
    auto lm_items_dir = out_filepath.parent_path() / "_libman";
    std::vector<lm::pair> kvs;
    kvs.emplace_back("Type", "Index");
    for (const package_plan& pkg : plan.build_packages) {
        auto pkg_lmp = generate_lmp(pkg, lm_items_dir, env);
        kvs.emplace_back("Package", fmt::format(
            "{}; {}",
            pkg.name,
            fs::relative(pkg_lmp, out_filepath.parent_path()).string()
        ));
    }
    lm::write_pairs(out_filepath, kvs);
}