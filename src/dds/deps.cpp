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

using sdist_index_type = std::map<std::string, std::reference_wrapper<const sdist>>;
using sdist_names      = std::set<std::string>;

namespace {

void resolve_ureqs_(shared_compile_file_rules& rules,
                    const package_manifest&    man,
                    const sdist_index_type&    sd_idx) {
    for (const dependency& dep : man.dependencies) {
        auto found = sd_idx.find(dep.name);
        if (found == sd_idx.end()) {
            throw std::runtime_error(
                fmt::format("Unable to resolve dependency '{}' (required by '{}')",
                            dep.name,
                            man.pk_id.to_string()));
        }
        resolve_ureqs_(rules, found->second.get().manifest, sd_idx);
        auto lib_src     = found->second.get().path / "src";
        auto lib_include = found->second.get().path / "include";
        if (fs::exists(lib_include)) {
            rules.include_dirs().push_back(lib_include);
        } else {
            rules.include_dirs().push_back(lib_src);
        }
    }
}

void resolve_ureqs(shared_compile_file_rules rules,
                   const sdist&              sd,
                   const library&            lib,
                   const library_plan&       lib_plan,
                   build_env_ref             env,
                   usage_requirement_map&    ureqs) {
    // Add the transitive requirements for this library to our compile rules.
    for (auto&& use : lib.manifest().uses) {
        ureqs.apply(rules, use.namespace_, use.name);
    }

    // Create usage requirements for this libary.
    lm::library& reqs = ureqs.add(sd.manifest.namespace_, lib.manifest().name);
    reqs.include_paths.push_back(lib.public_include_dir());
    reqs.name  = lib.manifest().name;
    reqs.uses  = lib.manifest().uses;
    reqs.links = lib.manifest().links;
    if (lib_plan.create_archive()) {
        reqs.linkable_path = lib_plan.create_archive()->calc_archive_file_path(env);
    }
    // TODO: preprocessor definitions
}

void add_sdist_to_dep_plan(build_plan&             plan,
                           const sdist&            sd,
                           build_env_ref           env,
                           const sdist_index_type& sd_idx,
                           usage_requirement_map&  ureqs,
                           sdist_names&            already_added) {
    if (already_added.find(sd.manifest.pk_id.name) != already_added.end()) {
        // We've already loaded this package into the plan.
        return;
    }
    spdlog::debug("Add to plan: {}", sd.manifest.pk_id.name);
    // First, load every dependency
    for (const auto& dep : sd.manifest.dependencies) {
        auto other = sd_idx.find(dep.name);
        assert(other != sd_idx.end()
               && "Failed to load a transitive dependency shortly after initializing them. What?");
        add_sdist_to_dep_plan(plan, other->second, env, sd_idx, ureqs, already_added);
    }
    // Record that we have been processed:
    already_added.insert(sd.manifest.pk_id.name);
    // Add the package:
    auto& pkg  = plan.add_package(package_plan(sd.manifest.pk_id.name, sd.manifest.namespace_));
    auto  libs = collect_libraries(sd.path);
    for (const auto& lib : libs) {
        shared_compile_file_rules comp_rules = lib.base_compile_rules();
        library_build_params      params;
        auto                      lib_plan = library_plan::create(lib, params, ureqs);
        resolve_ureqs(comp_rules, sd, lib, lib_plan, env, ureqs);
        pkg.add_library(std::move(lib_plan));
    }
}

}  // namespace

build_plan dds::create_deps_build_plan(const std::vector<sdist>& deps, build_env_ref env) {
    auto sd_idx = deps  //
        | ranges::views::transform([](const auto& sd) {
                      return std::pair(sd.manifest.pk_id.name, std::cref(sd));
                  })  //
        | ranges::to<sdist_index_type>();

    build_plan            plan;
    usage_requirement_map ureqs;
    sdist_names           already_added;
    for (const sdist& sd : deps) {
        spdlog::info("Recording dependency: {}", sd.manifest.pk_id.name);
        add_sdist_to_dep_plan(plan, sd, env, sd_idx, ureqs, already_added);
    }
    return plan;
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